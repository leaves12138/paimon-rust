// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

use crate::io::FileIO;
use crate::spec::stats::BinaryTableStats;
use crate::spec::{
    datums_to_binary_row, extract_datum, merge_active_entries, BinaryRow, CommitKind, CoreOptions,
    Datum, FileKind, GlobalIndexMeta, IndexManifest, Manifest, ManifestEntry, ManifestFileMeta,
    ManifestList, PartitionStatistics, Snapshot,
};
use crate::table::blob_file_writer::is_blob_file_name;
use crate::table::{RenamingSnapshotCommit, SnapshotCommit, SnapshotManager, Table};
use crate::Result;
use std::collections::{BTreeMap, HashMap, HashSet};
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};

const COMMIT_USER: &str = "paimon-rust-reassign-row-id";
const BATCH_COMMIT_IDENTIFIER: i64 = i64::MAX;
const SPECIAL_ROW_ID_FIELD: &str = "_ROW_ID";

/// Reassigns row IDs for data-evolution tables by rewriting metadata only.
#[derive(Clone)]
pub struct DataEvolutionRowIdReassigner {
    table: Table,
    snapshot_manager: SnapshotManager,
    snapshot_commit: Arc<dyn SnapshotCommit>,
}

impl DataEvolutionRowIdReassigner {
    pub fn new(table: Table) -> Self {
        let snapshot_manager = SnapshotManager::new(table.file_io.clone(), table.location.clone());
        let snapshot_commit = if let Some(env) = &table.rest_env {
            env.snapshot_commit()
        } else {
            Arc::new(RenamingSnapshotCommit::new(snapshot_manager.clone()))
        };
        Self {
            table,
            snapshot_manager,
            snapshot_commit,
        }
    }

    /// Reassign row IDs using the default commit user.
    pub async fn reassign(&self) -> Result<ReassignRowIdResult> {
        self.reassign_with_commit_user(COMMIT_USER).await
    }

    /// Reassign row IDs using the supplied commit user.
    pub async fn reassign_with_commit_user(
        &self,
        commit_user: impl Into<String>,
    ) -> Result<ReassignRowIdResult> {
        let core_options = CoreOptions::new(self.table.schema().options());
        if !core_options.row_tracking_enabled() {
            return Err(crate::Error::DataInvalid {
                message: format!(
                    "Table '{}' must enable 'row-tracking.enabled=true' before reassigning row IDs.",
                    self.table.identifier()
                ),
                source: None,
            });
        }
        if !core_options.data_evolution_enabled() {
            return Err(crate::Error::DataInvalid {
                message: format!(
                    "Table '{}' must enable 'data-evolution.enabled=true' before reassigning row IDs.",
                    self.table.identifier()
                ),
                source: None,
            });
        }

        let latest = match self.snapshot_manager.get_latest_snapshot().await? {
            Some(snapshot) => snapshot,
            None => {
                return Err(crate::Error::DataInvalid {
                    message: format!(
                        "Cannot reassign row IDs for empty table '{}'.",
                        self.table.identifier()
                    ),
                    source: None,
                });
            }
        };
        let first_assigned_row_id =
            latest
                .next_row_id()
                .ok_or_else(|| crate::Error::DataInvalid {
                    message: format!(
                        "Next row id cannot be null for row-tracking table '{}'.",
                        self.table.identifier()
                    ),
                    source: None,
                })?;

        if self.table.schema().partition_keys().is_empty() {
            return Ok(ReassignRowIdResult::skipped(
                latest.id(),
                first_assigned_row_id,
                "table is not partitioned",
            ));
        }

        let manifest_metas = self.read_data_manifest_metas(&latest).await?;
        let current_entries = self.current_entries(&manifest_metas).await?;
        if current_entries.is_empty() {
            return Ok(ReassignRowIdResult::skipped(
                latest.id(),
                first_assigned_row_id,
                "table has no current files",
            ));
        }

        let assignment = self.plan_assignment(current_entries, first_assigned_row_id)?;
        if assignment.reassigned_file_count == 0 {
            return Ok(ReassignRowIdResult::skipped(
                latest.id(),
                first_assigned_row_id,
                "partition row IDs are already contiguous",
            ));
        }

        let manifest_dir = self.snapshot_manager.manifest_dir();
        let file_io = self.snapshot_manager.file_io();
        let rewritten_manifest_metas = self
            .rewrite_manifests(file_io, &manifest_dir, &manifest_metas, &assignment)
            .await?;
        let base_manifest_list_name = format!("manifest-list-{}-0", uuid::Uuid::new_v4());
        let delta_manifest_list_name = format!("manifest-list-{}-1", uuid::Uuid::new_v4());
        ManifestList::write(
            file_io,
            &format!("{manifest_dir}/{base_manifest_list_name}"),
            &rewritten_manifest_metas,
        )
        .await?;
        ManifestList::write(
            file_io,
            &format!("{manifest_dir}/{delta_manifest_list_name}"),
            &[],
        )
        .await?;

        let rewritten_index_manifest = self.rewrite_index_manifest(&latest, &assignment).await?;
        let new_snapshot_id = latest.id() + 1;
        let snapshot = Snapshot::builder()
            .version(3)
            .id(new_snapshot_id)
            .schema_id(latest.schema_id())
            .base_manifest_list(base_manifest_list_name)
            .delta_manifest_list(delta_manifest_list_name)
            .commit_user(commit_user.into())
            .commit_identifier(BATCH_COMMIT_IDENTIFIER)
            .commit_kind(CommitKind::OVERWRITE)
            .time_millis(current_time_millis())
            .total_record_count(latest.total_record_count())
            .delta_record_count(Some(0))
            .watermark(latest.watermark())
            .statistics(latest.statistics().map(str::to_string))
            .next_row_id(Some(assignment.next_row_id))
            .index_manifest(rewritten_index_manifest.index_manifest)
            .build();

        let committed = self
            .snapshot_commit
            .commit(&snapshot, &[] as &[PartitionStatistics])
            .await?;
        if !committed {
            return Err(crate::Error::DataInvalid {
                message: "Failed to reassign row IDs because a newer snapshot has been committed."
                    .to_string(),
                source: None,
            });
        }

        Ok(ReassignRowIdResult {
            previous_snapshot_id: latest.id(),
            new_snapshot_id,
            file_count: assignment.reassigned_file_count,
            row_count: assignment.logical_row_count,
            index_file_count: rewritten_index_manifest.index_file_count,
            first_assigned_row_id,
            next_row_id: assignment.next_row_id,
            reassigned: true,
            skip_reason: None,
        })
    }

    async fn read_data_manifest_metas(&self, snapshot: &Snapshot) -> Result<Vec<ManifestFileMeta>> {
        let manifest_dir = self.snapshot_manager.manifest_dir();
        let file_io = self.snapshot_manager.file_io();
        let mut metas = ManifestList::read(
            file_io,
            &format!("{manifest_dir}/{}", snapshot.base_manifest_list()),
        )
        .await?;
        let mut delta = ManifestList::read(
            file_io,
            &format!("{manifest_dir}/{}", snapshot.delta_manifest_list()),
        )
        .await?;
        metas.append(&mut delta);
        Ok(metas)
    }

    async fn current_entries(
        &self,
        manifest_metas: &[ManifestFileMeta],
    ) -> Result<Vec<SourcedManifestEntry>> {
        let mut all_entries = Vec::new();
        let manifest_dir = self.snapshot_manager.manifest_dir();
        let file_io = self.snapshot_manager.file_io();
        for manifest_meta in manifest_metas {
            if manifest_meta.num_added_files() + manifest_meta.num_deleted_files() <= 0 {
                continue;
            }
            let path = format!("{manifest_dir}/{}", manifest_meta.file_name());
            let entries = Manifest::read(file_io, &path).await?;
            for entry in entries {
                all_entries.push(SourcedManifestEntry {
                    manifest_name: manifest_meta.file_name().to_string(),
                    entry,
                });
            }
        }

        let active_entries = merge_active_entries(
            all_entries
                .iter()
                .map(|sourced| sourced.entry.clone())
                .collect(),
        );
        let active_identifiers: HashSet<_> = active_entries
            .into_iter()
            .map(|entry| entry.identifier())
            .collect();

        Ok(all_entries
            .into_iter()
            .filter(|sourced| {
                *sourced.entry.kind() == FileKind::Add
                    && active_identifiers.contains(&sourced.entry.identifier())
            })
            .collect())
    }

    fn plan_assignment(
        &self,
        current_entries: Vec<SourcedManifestEntry>,
        first_row_id: i64,
    ) -> Result<Assignment> {
        let mut entries_by_partition: BTreeMap<Vec<u8>, Vec<SourcedManifestEntry>> =
            BTreeMap::new();
        for sourced in current_entries {
            validate_reassignable(&sourced.entry, self.table.identifier())?;
            entries_by_partition
                .entry(sourced.entry.partition().to_vec())
                .or_default()
                .push(sourced);
        }

        for entries in entries_by_partition.values_mut() {
            entries.sort_by(|left, right| compare_entries(&left.entry, &right.entry));
        }

        let mut reassigned_entries_by_identifier = HashMap::new();
        let mut manifest_names_to_rewrite = HashSet::new();
        let mut row_id_mappings = HashMap::new();
        let mut next_row_id = first_row_id;
        let mut logical_row_count = 0;
        let mut reassigned_file_count = 0;

        for (partition, sourced_entries) in entries_by_partition {
            let entries: Vec<_> = sourced_entries
                .iter()
                .map(|sourced| sourced.entry.clone())
                .collect();
            if partition_row_ids_are_contiguous(&entries)? {
                continue;
            }

            let partition_first_row_id = next_row_id;
            let assignment = assign_partition(entries, next_row_id)?;
            next_row_id = assignment.next_row_id;
            logical_row_count += next_row_id - partition_first_row_id;
            reassigned_file_count += assignment.entries.len() as i64;
            row_id_mappings.insert(partition, assignment.row_id_mappings);

            for sourced in sourced_entries {
                manifest_names_to_rewrite.insert(sourced.manifest_name);
            }
            for entry in assignment.entries {
                let previous = reassigned_entries_by_identifier.insert(entry.identifier(), entry);
                if previous.is_some() {
                    return Err(crate::Error::DataInvalid {
                        message: "Duplicate current manifest entry while reassigning row IDs."
                            .to_string(),
                        source: None,
                    });
                }
            }
        }

        Ok(Assignment {
            reassigned_entries_by_identifier,
            manifest_names_to_rewrite,
            row_id_mappings,
            next_row_id,
            logical_row_count,
            reassigned_file_count,
        })
    }

    async fn rewrite_manifests(
        &self,
        file_io: &FileIO,
        manifest_dir: &str,
        manifest_metas: &[ManifestFileMeta],
        assignment: &Assignment,
    ) -> Result<Vec<ManifestFileMeta>> {
        let mut rewritten_manifest_metas = Vec::with_capacity(manifest_metas.len());
        for manifest_meta in manifest_metas {
            if !assignment
                .manifest_names_to_rewrite
                .contains(manifest_meta.file_name())
            {
                rewritten_manifest_metas.push(manifest_meta.clone());
                continue;
            }

            let old_manifest_path = format!("{manifest_dir}/{}", manifest_meta.file_name());
            let entries = Manifest::read(file_io, &old_manifest_path).await?;
            let rewritten_entries = entries
                .into_iter()
                .map(|entry| {
                    if *entry.kind() == FileKind::Add {
                        if let Some(reassigned) = assignment
                            .reassigned_entries_by_identifier
                            .get(&entry.identifier())
                        {
                            return reassigned.clone();
                        }
                    }
                    entry
                })
                .collect::<Vec<_>>();

            let new_manifest_name = format!("manifest-{}-0", uuid::Uuid::new_v4());
            let new_manifest_path = format!("{manifest_dir}/{new_manifest_name}");
            let rewritten_manifest_meta = self
                .write_manifest_file(
                    file_io,
                    &new_manifest_path,
                    &new_manifest_name,
                    &rewritten_entries,
                )
                .await?;
            rewritten_manifest_metas.push(rewritten_manifest_meta);
        }
        Ok(rewritten_manifest_metas)
    }

    async fn write_manifest_file(
        &self,
        file_io: &FileIO,
        path: &str,
        file_name: &str,
        entries: &[ManifestEntry],
    ) -> Result<ManifestFileMeta> {
        Manifest::write(file_io, path, entries).await?;

        let mut added_file_count = 0;
        let mut deleted_file_count = 0;
        let mut min_row_id: Option<i64> = None;
        let mut max_row_id: Option<i64> = None;
        for entry in entries {
            match entry.kind() {
                FileKind::Add => added_file_count += 1,
                FileKind::Delete => deleted_file_count += 1,
            }
            if let Some((start, end)) = entry.file().row_id_range() {
                min_row_id = Some(min_row_id.map_or(start, |current| current.min(start)));
                max_row_id = Some(max_row_id.map_or(end, |current| current.max(end)));
            }
        }

        let status = file_io.get_status(path).await?;
        Ok(ManifestFileMeta::new_with_version(
            3,
            file_name.to_string(),
            status.size as i64,
            added_file_count,
            deleted_file_count,
            self.compute_partition_stats(entries)?,
            self.table.schema().id(),
            min_row_id,
            max_row_id,
        ))
    }

    fn compute_partition_stats(&self, entries: &[ManifestEntry]) -> Result<BinaryTableStats> {
        let partition_fields = self.table.schema().partition_fields();
        let num_fields = partition_fields.len();

        if num_fields == 0 || entries.is_empty() {
            return Ok(BinaryTableStats::new(vec![], vec![], vec![]));
        }

        let data_types: Vec<_> = partition_fields
            .iter()
            .map(|field| field.data_type().clone())
            .collect();
        let mut mins: Vec<Option<Datum>> = vec![None; num_fields];
        let mut maxs: Vec<Option<Datum>> = vec![None; num_fields];
        let mut null_counts: Vec<i64> = vec![0; num_fields];

        for entry in entries {
            let partition_bytes = entry.partition();
            if partition_bytes.is_empty() {
                continue;
            }
            let row = BinaryRow::from_serialized_bytes(partition_bytes)?;
            for (index, data_type) in data_types.iter().enumerate() {
                match extract_datum(&row, index, data_type)? {
                    Some(datum) => {
                        mins[index] = Some(match mins[index].take() {
                            Some(current) if current <= datum => current,
                            Some(_) => datum.clone(),
                            None => datum.clone(),
                        });
                        maxs[index] = Some(match maxs[index].take() {
                            Some(current) if current >= datum => current,
                            Some(_) => datum,
                            None => datum,
                        });
                    }
                    None => {
                        null_counts[index] += 1;
                    }
                }
            }
        }

        let min_datums: Vec<_> = mins.iter().zip(data_types.iter()).collect();
        let max_datums: Vec<_> = maxs.iter().zip(data_types.iter()).collect();
        Ok(BinaryTableStats::new(
            datums_to_binary_row(&min_datums),
            datums_to_binary_row(&max_datums),
            null_counts.into_iter().map(Some).collect(),
        ))
    }

    async fn rewrite_index_manifest(
        &self,
        latest: &Snapshot,
        assignment: &Assignment,
    ) -> Result<RewrittenIndexManifest> {
        let Some(index_manifest) = latest.index_manifest() else {
            return Ok(RewrittenIndexManifest {
                index_manifest: None,
                index_file_count: 0,
            });
        };

        let manifest_dir = self.snapshot_manager.manifest_dir();
        let file_io = self.snapshot_manager.file_io();
        let index_entries =
            IndexManifest::read(file_io, &format!("{manifest_dir}/{index_manifest}")).await?;
        if index_entries.is_empty() {
            return Ok(RewrittenIndexManifest {
                index_manifest: None,
                index_file_count: 0,
            });
        }

        let mut rewritten = Vec::with_capacity(index_entries.len());
        let mut index_file_count = 0;
        for mut entry in index_entries {
            if entry.kind != FileKind::Add {
                return Err(crate::Error::DataInvalid {
                    message: format!(
                        "Index manifest '{index_manifest}' contains non-current entry {entry:?}."
                    ),
                    source: None,
                });
            }

            let Some(mapping_index) = assignment.row_id_mappings.get(&entry.partition) else {
                rewritten.push(entry);
                continue;
            };
            let Some(global_index) = entry.index_file.global_index_meta.as_ref() else {
                rewritten.push(entry);
                continue;
            };

            let new_range = mapping_index.map(RowRange::new(
                global_index.row_range_start,
                global_index.row_range_end,
            ))?;
            entry.index_file.global_index_meta = Some(GlobalIndexMeta {
                row_range_start: new_range.start,
                row_range_end: new_range.end,
                index_field_id: global_index.index_field_id,
                extra_field_ids: global_index.extra_field_ids.clone(),
                index_meta: global_index.index_meta.clone(),
            });
            index_file_count += 1;
            rewritten.push(entry);
        }

        let new_index_manifest_name = format!("index-manifest-{}-0", uuid::Uuid::new_v4());
        IndexManifest::write(
            file_io,
            &format!("{manifest_dir}/{new_index_manifest_name}"),
            &rewritten,
        )
        .await?;
        Ok(RewrittenIndexManifest {
            index_manifest: Some(new_index_manifest_name),
            index_file_count,
        })
    }
}

/// Result of row-id reassignment.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ReassignRowIdResult {
    pub previous_snapshot_id: i64,
    pub new_snapshot_id: i64,
    pub file_count: i64,
    pub row_count: i64,
    pub index_file_count: i64,
    pub first_assigned_row_id: i64,
    pub next_row_id: i64,
    pub reassigned: bool,
    pub skip_reason: Option<String>,
}

impl ReassignRowIdResult {
    fn skipped(snapshot_id: i64, next_row_id: i64, reason: impl Into<String>) -> Self {
        Self {
            previous_snapshot_id: snapshot_id,
            new_snapshot_id: snapshot_id,
            file_count: 0,
            row_count: 0,
            index_file_count: 0,
            first_assigned_row_id: next_row_id,
            next_row_id,
            reassigned: false,
            skip_reason: Some(reason.into()),
        }
    }
}

#[derive(Debug, Clone)]
struct SourcedManifestEntry {
    manifest_name: String,
    entry: ManifestEntry,
}

#[derive(Debug)]
struct Assignment {
    reassigned_entries_by_identifier: HashMap<crate::spec::Identifier, ManifestEntry>,
    manifest_names_to_rewrite: HashSet<String>,
    row_id_mappings: HashMap<Vec<u8>, RowRangeMappingIndex>,
    next_row_id: i64,
    logical_row_count: i64,
    reassigned_file_count: i64,
}

#[derive(Debug)]
struct PartitionAssignment {
    entries: Vec<ManifestEntry>,
    row_id_mappings: RowRangeMappingIndex,
    next_row_id: i64,
}

#[derive(Debug)]
struct RewrittenIndexManifest {
    index_manifest: Option<String>,
    index_file_count: i64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
struct RowRange {
    start: i64,
    end: i64,
}

impl RowRange {
    fn new(start: i64, end: i64) -> Self {
        Self { start, end }
    }

    fn count(&self) -> i64 {
        self.end - self.start + 1
    }

    fn overlaps(&self, other: &Self) -> bool {
        self.start <= other.end && other.start <= self.end
    }

    fn span(&self, other: &Self) -> Self {
        Self {
            start: self.start.min(other.start),
            end: self.end.max(other.end),
        }
    }
}

#[derive(Debug)]
struct RowRangeMappingIndex {
    mappings: Vec<RowRangeMapping>,
    old_ends: Vec<i64>,
}

impl RowRangeMappingIndex {
    fn new(mut mappings: Vec<RowRangeMapping>) -> Result<Self> {
        if mappings.is_empty() {
            return Err(crate::Error::DataInvalid {
                message: "Row range mappings cannot be empty.".to_string(),
                source: None,
            });
        }
        mappings.sort_by_key(|mapping| mapping.old_range.start);
        let mut previous: Option<RowRangeMapping> = None;
        for mapping in &mappings {
            if mapping.old_range.start > mapping.old_range.end {
                return Err(crate::Error::DataInvalid {
                    message: format!(
                        "Invalid old row range [{}, {}].",
                        mapping.old_range.start, mapping.old_range.end
                    ),
                    source: None,
                });
            }
            if let Some(previous) = previous {
                if previous.old_range.end >= mapping.old_range.start {
                    return Err(crate::Error::DataInvalid {
                        message: "Old row range mappings cannot overlap.".to_string(),
                        source: None,
                    });
                }
            }
            previous = Some(*mapping);
        }
        let old_ends = mappings
            .iter()
            .map(|mapping| mapping.old_range.end)
            .collect();
        Ok(Self { mappings, old_ends })
    }

    fn map(&self, old_range: RowRange) -> Result<RowRange> {
        if old_range.start > old_range.end {
            return Err(crate::Error::DataInvalid {
                message: format!(
                    "Invalid old row range [{}, {}].",
                    old_range.start, old_range.end
                ),
                source: None,
            });
        }

        let mut cursor = old_range.start;
        let mut new_from = None;
        let mut new_to = i64::MIN;
        for mapping in self
            .mappings
            .iter()
            .skip(self.old_ends.partition_point(|end| *end < cursor))
        {
            if mapping.old_range.start > cursor {
                break;
            }

            let segment_to = mapping.old_range.end.min(old_range.end);
            let segment_new_from = mapping.new_start + cursor - mapping.old_range.start;
            let segment_new_to = mapping.new_start + segment_to - mapping.old_range.start;
            if new_from.is_some() {
                if new_to + 1 != segment_new_from {
                    return Err(crate::Error::DataInvalid {
                        message: format!(
                            "Global index row range {:?} maps to non-contiguous new row range.",
                            old_range
                        ),
                        source: None,
                    });
                }
            } else {
                new_from = Some(segment_new_from);
            }
            new_to = segment_new_to;
            cursor = segment_to + 1;
            if cursor > old_range.end {
                break;
            }
        }

        if cursor <= old_range.end || new_from.is_none() {
            return Err(crate::Error::DataInvalid {
                message: format!(
                    "Global index row range {:?} is not fully covered by data file row-id mappings.",
                    old_range
                ),
                source: None,
            });
        }

        Ok(RowRange::new(new_from.unwrap(), new_to))
    }
}

#[derive(Debug, Clone, Copy)]
struct RowRangeMapping {
    old_range: RowRange,
    new_start: i64,
}

fn validate_reassignable(entry: &ManifestEntry, table_name: &impl std::fmt::Display) -> Result<()> {
    if entry
        .file()
        .write_cols
        .as_ref()
        .is_some_and(|write_cols| write_cols.iter().any(|col| col == SPECIAL_ROW_ID_FIELD))
    {
        return Err(crate::Error::DataInvalid {
            message: format!(
                "Cannot reassign row IDs for file '{}' because it physically stores the row-id field.",
                entry.file().file_name
            ),
            source: None,
        });
    }
    if entry.file().first_row_id.is_none() {
        return Err(crate::Error::DataInvalid {
            message: format!(
                "File '{}' in table '{}' does not have first row id.",
                entry.file().file_name,
                table_name
            ),
            source: None,
        });
    }
    Ok(())
}

fn partition_row_ids_are_contiguous(entries: &[ManifestEntry]) -> Result<bool> {
    let mut logical_ranges = logical_ranges(entries)?;
    if logical_ranges.len() <= 1 {
        return Ok(true);
    }
    logical_ranges.sort_by_key(|range| (range.start, range.end));
    let mut previous_end = logical_ranges[0].end;
    for range in logical_ranges.into_iter().skip(1) {
        if range.start != previous_end + 1 {
            return Ok(false);
        }
        previous_end = range.end;
    }
    Ok(true)
}

fn logical_ranges(entries: &[ManifestEntry]) -> Result<Vec<RowRange>> {
    merge_overlapping_range_groups(entries)?
        .into_iter()
        .map(|group| old_logical_range(&group))
        .collect()
}

fn assign_partition(entries: Vec<ManifestEntry>, first_row_id: i64) -> Result<PartitionAssignment> {
    let groups = merge_overlapping_range_groups(&entries)?;
    let mut reassigned = Vec::with_capacity(entries.len());
    let mut mappings = Vec::with_capacity(groups.len());
    let mut next_row_id = first_row_id;

    for mut group in groups {
        group.sort_by(compare_entries_without_partition);
        let old_logical_range = old_logical_range(&group)?;
        mappings.push(RowRangeMapping {
            old_range: old_logical_range,
            new_start: next_row_id,
        });

        for entry in group {
            let old_first_row_id = entry.file().first_row_id.unwrap();
            let new_first_row_id = next_row_id + old_first_row_id - old_logical_range.start;
            reassigned.push(entry.with_first_row_id(new_first_row_id));
        }
        next_row_id += old_logical_range.count();
    }

    Ok(PartitionAssignment {
        entries: reassigned,
        row_id_mappings: RowRangeMappingIndex::new(mappings)?,
        next_row_id,
    })
}

fn merge_overlapping_range_groups(entries: &[ManifestEntry]) -> Result<Vec<Vec<ManifestEntry>>> {
    let mut sorted = entries.to_vec();
    sorted.sort_by_key(|entry| {
        let range = non_null_row_id_range(entry).unwrap_or(RowRange::new(i64::MAX, i64::MAX));
        (range.start, range.end, entry.file().file_name.clone())
    });

    let mut groups: Vec<(RowRange, Vec<ManifestEntry>)> = Vec::new();
    for entry in sorted {
        let range = non_null_row_id_range(&entry)?;
        match groups.last_mut() {
            Some((current_range, current_group)) if current_range.overlaps(&range) => {
                *current_range = current_range.span(&range);
                current_group.push(entry);
            }
            _ => groups.push((range, vec![entry])),
        }
    }
    Ok(groups.into_iter().map(|(_, group)| group).collect())
}

fn old_logical_range(group: &[ManifestEntry]) -> Result<RowRange> {
    let data_files: Vec<_> = group
        .iter()
        .filter(|entry| !is_special_file(entry))
        .collect();
    let logical_range = if data_files.is_empty() {
        spanning_range(group)?
    } else {
        let logical_range = non_null_row_id_range(data_files[0])?;
        for data_file in data_files {
            let current = non_null_row_id_range(data_file)?;
            if logical_range != current {
                return Err(crate::Error::DataInvalid {
                    message: format!(
                        "Data files in one overlapping row-id group must have the same row-id range, but found {:?} and {:?}.",
                        logical_range, current
                    ),
                    source: None,
                });
            }
        }
        logical_range
    };

    for entry in group {
        let range = non_null_row_id_range(entry)?;
        if range.start < logical_range.start || range.end > logical_range.end {
            return Err(crate::Error::DataInvalid {
                message: format!(
                    "File '{}' row-id range {:?} is outside logical row-id range {:?}.",
                    entry.file().file_name,
                    range,
                    logical_range
                ),
                source: None,
            });
        }
    }
    Ok(logical_range)
}

fn spanning_range(group: &[ManifestEntry]) -> Result<RowRange> {
    let mut min = i64::MAX;
    let mut max = i64::MIN;
    for entry in group {
        let range = non_null_row_id_range(entry)?;
        min = min.min(range.start);
        max = max.max(range.end);
    }
    Ok(RowRange::new(min, max))
}

fn non_null_row_id_range(entry: &ManifestEntry) -> Result<RowRange> {
    let (start, end) = entry
        .file()
        .row_id_range()
        .ok_or_else(|| crate::Error::DataInvalid {
            message: format!(
                "File '{}' does not have first row id.",
                entry.file().file_name
            ),
            source: None,
        })?;
    Ok(RowRange::new(start, end))
}

fn compare_entries(left: &ManifestEntry, right: &ManifestEntry) -> std::cmp::Ordering {
    left.partition()
        .cmp(right.partition())
        .then_with(|| compare_entries_without_partition(left, right))
}

fn compare_entries_without_partition(
    left: &ManifestEntry,
    right: &ManifestEntry,
) -> std::cmp::Ordering {
    left.file()
        .first_row_id
        .cmp(&right.file().first_row_id)
        .then_with(|| file_order(left).cmp(&file_order(right)))
        .then_with(|| {
            right
                .file()
                .max_sequence_number
                .cmp(&left.file().max_sequence_number)
        })
        .then_with(|| left.file().file_name.cmp(&right.file().file_name))
}

fn file_order(entry: &ManifestEntry) -> i32 {
    if is_blob_file_name(&entry.file().file_name) {
        1
    } else if is_vector_store_file_name(&entry.file().file_name) {
        2
    } else {
        0
    }
}

fn is_special_file(entry: &ManifestEntry) -> bool {
    is_blob_file_name(&entry.file().file_name) || is_vector_store_file_name(&entry.file().file_name)
}

fn is_vector_store_file_name(file_name: &str) -> bool {
    file_name.contains("vector") || file_name.contains("VECTOR")
}

fn current_time_millis() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis() as u64
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::spec::{DataFileMeta, ManifestEntry};

    fn file(name: &str, first_row_id: i64, row_count: i64) -> DataFileMeta {
        let stats = BinaryTableStats::new(vec![], vec![], vec![]);
        DataFileMeta {
            file_name: name.to_string(),
            file_size: 100,
            row_count,
            min_key: vec![],
            max_key: vec![],
            key_stats: stats.clone(),
            value_stats: stats,
            min_sequence_number: 0,
            max_sequence_number: 0,
            schema_id: 0,
            level: 0,
            extra_files: vec![],
            creation_time: None,
            delete_row_count: None,
            embedded_index: None,
            file_source: None,
            value_stats_cols: None,
            external_path: None,
            first_row_id: Some(first_row_id),
            write_cols: None,
        }
    }

    fn entry(partition: &[u8], name: &str, first_row_id: i64, row_count: i64) -> ManifestEntry {
        ManifestEntry::new(
            FileKind::Add,
            partition.to_vec(),
            0,
            1,
            file(name, first_row_id, row_count),
            2,
        )
    }

    #[test]
    fn test_partition_row_ids_are_contiguous() {
        let entries = vec![entry(b"p", "a", 0, 100), entry(b"p", "b", 100, 100)];
        assert!(partition_row_ids_are_contiguous(&entries).unwrap());

        let entries = vec![entry(b"p", "a", 0, 100), entry(b"p", "b", 200, 100)];
        assert!(!partition_row_ids_are_contiguous(&entries).unwrap());
    }

    #[test]
    fn test_assign_partition_preserves_offsets_and_closes_gaps() {
        let entries = vec![entry(b"p", "a", 0, 100), entry(b"p", "b", 200, 100)];
        let assignment = assign_partition(entries, 1000).unwrap();
        assert_eq!(assignment.next_row_id, 1200);
        let mut starts = assignment
            .entries
            .iter()
            .map(|entry| entry.file().first_row_id.unwrap())
            .collect::<Vec<_>>();
        starts.sort_unstable();
        assert_eq!(starts, vec![1000, 1100]);
    }

    #[test]
    fn test_assign_partition_keeps_overlapping_special_file_offset() {
        let entries = vec![
            entry(b"p", "data.parquet", 100, 100),
            entry(b"p", "data.blob", 120, 20),
            entry(b"p", "other.parquet", 300, 100),
        ];
        let assignment = assign_partition(entries, 1000).unwrap();
        let starts: HashMap<_, _> = assignment
            .entries
            .iter()
            .map(|entry| {
                (
                    entry.file().file_name.as_str(),
                    entry.file().first_row_id.unwrap(),
                )
            })
            .collect();
        assert_eq!(starts["data.parquet"], 1000);
        assert_eq!(starts["data.blob"], 1020);
        assert_eq!(starts["other.parquet"], 1100);
    }

    #[test]
    fn test_row_range_mapping_index_maps_multi_segment_range() {
        let index = RowRangeMappingIndex::new(vec![
            RowRangeMapping {
                old_range: RowRange::new(0, 99),
                new_start: 1000,
            },
            RowRangeMapping {
                old_range: RowRange::new(200, 299),
                new_start: 1100,
            },
        ])
        .unwrap();
        assert_eq!(
            index.map(RowRange::new(0, 99)).unwrap(),
            RowRange::new(1000, 1099)
        );
        assert!(index.map(RowRange::new(50, 249)).is_err());
    }
}
