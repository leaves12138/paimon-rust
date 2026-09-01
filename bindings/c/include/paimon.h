// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#ifndef PAIMON_C_H
#define PAIMON_C_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define PAIMON_ERROR_ALREADY_EXISTS 3

#define PAIMON_ERROR_INVALID_INPUT 4

#define PAIMON_ERROR_IO 5

#define PAIMON_ERROR_NOT_FOUND 2

#define PAIMON_ERROR_OUT_OF_RANGE 6

#define PAIMON_ERROR_UNEXPECTED 0

#define PAIMON_ERROR_UNSUPPORTED 1

#define PAIMON_STREAM_FOLLOW_UP_AUTO 0

#define PAIMON_STREAM_FOLLOW_UP_CHANGELOG 2

#define PAIMON_STREAM_FOLLOW_UP_DELTA 1

#define PAIMON_STREAM_POLL_DATA 0

#define PAIMON_STREAM_POLL_END 2

#define PAIMON_STREAM_POLL_WAITING 1

#define PAIMON_STREAM_READ_AUDIT_LOG 1

#define PAIMON_STREAM_READ_DATA 0

#define PAIMON_STREAM_STARTUP_FROM_SNAPSHOT 2

#define PAIMON_STREAM_STARTUP_FROM_SNAPSHOT_FULL 3

#define PAIMON_STREAM_STARTUP_LATEST 1

#define PAIMON_STREAM_STARTUP_LATEST_FULL 0

/**
 * A single Arrow record batch exported via the Arrow C Data Interface.
 *
 * `array` and `schema` point to heap-allocated ArrowArray and ArrowSchema
 * structs. After importing the data, call `paimon_arrow_batch_free` to free
 * the container structs.
 */
typedef struct paimon_arrow_batch {
  /**
   * Pointer to a heap-allocated ArrowArray.
   */
  void *array;
  /**
   * Pointer to a heap-allocated ArrowSchema.
   */
  void *schema;
} paimon_arrow_batch;

typedef struct paimon_blob_reader {
  void *inner;
} paimon_blob_reader;

/**
 * C-compatible byte buffer.
 */
typedef struct paimon_bytes {
  uint8_t *data;
  size_t len;
} paimon_bytes;

/**
 * C-compatible error type.
 */
typedef struct paimon_error {
  int32_t code;
  struct paimon_bytes message;
} paimon_error;

typedef struct paimon_result_blob_reader {
  struct paimon_blob_reader *reader;
  struct paimon_error *error;
} paimon_result_blob_reader;

/**
 * C-compatible key-value pair for options.
 */
typedef struct paimon_option {
  const char *key;
  const char *value;
} paimon_option;

typedef struct paimon_blob_stream {
  void *inner;
} paimon_blob_stream;

typedef struct paimon_result_blob_stream {
  struct paimon_blob_stream *stream;
  struct paimon_error *error;
} paimon_result_blob_stream;

typedef struct paimon_bytes_array {
  struct paimon_bytes *data;
  size_t len;
} paimon_bytes_array;

typedef struct paimon_result_read_blobs {
  struct paimon_bytes_array blobs;
  struct paimon_error *error;
} paimon_result_read_blobs;

typedef struct paimon_byte_slice {
  const uint8_t *data;
  size_t len;
} paimon_byte_slice;

typedef struct paimon_result_blob_stream_read {
  size_t bytes_read;
  struct paimon_error *error;
} paimon_result_blob_stream_read;

typedef struct paimon_result_blob_stream_seek {
  uint64_t position;
  struct paimon_error *error;
} paimon_result_blob_stream_seek;

/**
 * Opaque wrapper around a heap-allocated Rust object.
 */
typedef struct paimon_catalog {
  void *inner;
} paimon_catalog;

typedef struct paimon_result_catalog_new {
  struct paimon_catalog *catalog;
  struct paimon_error *error;
} paimon_result_catalog_new;

typedef struct paimon_identifier {
  void *inner;
} paimon_identifier;

typedef struct paimon_table {
  void *inner;
} paimon_table;

typedef struct paimon_result_get_table {
  struct paimon_table *table;
  struct paimon_error *error;
} paimon_result_get_table;

/**
 * Opaque container for commit messages and their originating write context.
 */
typedef struct paimon_commit_messages {
  void *inner;
} paimon_commit_messages;

/**
 * Opaque durable prepared-commit handle for a standard table write.
 */
typedef struct paimon_prepared_commit {
  void *inner;
} paimon_prepared_commit;

typedef struct paimon_result_prepared_commit {
  struct paimon_prepared_commit *prepared;
  struct paimon_error *error;
} paimon_result_prepared_commit;

/**
 * Opaque wrapper around a cloneable Paimon FileIO.
 */
typedef struct paimon_file_io {
  void *inner;
} paimon_file_io;

typedef struct paimon_result_file_io_new {
  struct paimon_file_io *file_io;
  struct paimon_error *error;
} paimon_result_file_io_new;

/**
 * Version 1 callbacks for an externally managed file-block cache.
 *
 * Callbacks may run concurrently on arbitrary Rust runtime blocking threads.
 * They must not unwind across the C ABI. `get` returns the number of bytes
 * copied into `output`; return `-1` for a miss and any value other than the
 * requested length for a fail-open miss. All callback buffers and paths are
 * borrowed only for the duration of the call. Paths use pointer-plus-length
 * because canonical storage keys may contain embedded NUL separators.
 */
typedef struct paimon_file_cache_callbacks_v1 {
  void *context;
  int64_t (*get)(void *context,
                 const uint8_t *path_data,
                 size_t path_length,
                 uint64_t offset,
                 size_t length,
                 uint8_t *output);
  int32_t (*put)(void *context,
                 const uint8_t *path_data,
                 size_t path_length,
                 uint64_t offset,
                 const uint8_t *data,
                 size_t length);
  int32_t (*invalidate_path)(void *context, const uint8_t *path_data, size_t path_length);
  int32_t (*invalidate_prefix)(void *context, const uint8_t *prefix_data, size_t prefix_length);
  /**
   * Releases `context` after the last FileIO/table clone is dropped.
   */
  void (*destroy)(void *context);
} paimon_file_cache_callbacks_v1;

typedef struct paimon_result_identifier_new {
  struct paimon_identifier *identifier;
  struct paimon_error *error;
} paimon_result_identifier_new;

typedef struct paimon_plan {
  void *inner;
} paimon_plan;

typedef struct paimon_result_plan {
  struct paimon_plan *plan;
  struct paimon_error *error;
} paimon_result_plan;

typedef struct paimon_postpone_fixed_bucket_commit_messages {
  void *inner;
} paimon_postpone_fixed_bucket_commit_messages;

typedef struct paimon_postpone_fixed_bucket_table_commit {
  void *inner;
} paimon_postpone_fixed_bucket_table_commit;

typedef struct paimon_postpone_fixed_bucket_table_write {
  void *inner;
} paimon_postpone_fixed_bucket_table_write;

typedef struct paimon_result_postpone_fixed_bucket_prepare_commit {
  struct paimon_postpone_fixed_bucket_commit_messages *messages;
  struct paimon_error *error;
} paimon_result_postpone_fixed_bucket_prepare_commit;

typedef struct paimon_postpone_fixed_bucket_write_builder {
  void *inner;
} paimon_postpone_fixed_bucket_write_builder;

typedef struct paimon_result_postpone_fixed_bucket_table_commit {
  struct paimon_postpone_fixed_bucket_table_commit *commit;
  struct paimon_error *error;
} paimon_result_postpone_fixed_bucket_table_commit;

typedef struct paimon_result_postpone_fixed_bucket_table_write {
  struct paimon_postpone_fixed_bucket_table_write *write;
  struct paimon_error *error;
} paimon_result_postpone_fixed_bucket_table_write;

/**
 * Opaque wrapper around a Predicate.
 */
typedef struct paimon_predicate {
  void *inner;
} paimon_predicate;

typedef struct paimon_result_predicate {
  struct paimon_predicate *predicate;
  struct paimon_error *error;
} paimon_result_predicate;

/**
 * A typed literal value for predicate comparison, passed across FFI.
 *
 * # Design
 *
 * We use a tagged flat struct instead of opaque heap-allocated handles
 * (like DuckDB's `duckdb_value`). The trade-off:
 *
 * - **Pro**: Zero allocation — the entire datum is passed by value on the
 *   stack, with no heap round-trips or free calls needed. This keeps the
 *   FFI surface minimal and the Go/C caller simple.
 * - **Con**: The struct is larger than any single variant needs, wasting
 *   some bytes per datum (currently ~56 bytes vs. ~16 for the largest
 *   single variant).
 *
 * Since datums are only used for predicate construction (not a hot path),
 * the extra size is acceptable.
 *
 * # Tags
 *
 * - 0: Bool, 1: TinyInt, 2: SmallInt, 3: Int, 4: Long
 * - 5: Float, 6: Double, 7: String, 8: Date, 9: Time
 * - 10: Timestamp, 11: LocalZonedTimestamp, 12: Decimal, 13: Bytes
 *
 * `tag` determines which value fields are valid:
 * - `Bool`/`TinyInt`/`SmallInt`/`Int`/`Long`/`Date`/`Time` → `int_val`
 * - `Float`/`Double` → `double_val`
 * - `String`/`Bytes` → `str_data` + `str_len`
 * - `Timestamp`/`LocalZonedTimestamp` → `int_val` (millis) + `int_val2` (nanos)
 * - `Decimal` → `int_val` + `int_val2` (unscaled i128) + `uint_val` (precision) + `uint_val2` (scale)
 */
typedef struct paimon_datum {
  int32_t tag;
  int64_t int_val;
  double double_val;
  const uint8_t *str_data;
  size_t str_len;
  int64_t int_val2;
  uint32_t uint_val;
  uint32_t uint_val2;
} paimon_datum;

typedef struct paimon_result_bytes {
  struct paimon_bytes bytes;
  struct paimon_error *error;
} paimon_result_bytes;

typedef struct paimon_read_builder {
  void *inner;
} paimon_read_builder;

typedef struct paimon_table_read {
  void *inner;
} paimon_table_read;

typedef struct paimon_result_new_read {
  struct paimon_table_read *read;
  struct paimon_error *error;
} paimon_result_new_read;

typedef struct paimon_table_scan {
  void *inner;
} paimon_table_scan;

typedef struct paimon_result_table_scan {
  struct paimon_table_scan *scan;
  struct paimon_error *error;
} paimon_result_table_scan;

typedef struct paimon_stream_scan {
  void *inner;
} paimon_stream_scan;

typedef struct paimon_result_stream_scan {
  struct paimon_stream_scan *scan;
  struct paimon_error *error;
} paimon_result_stream_scan;

/**
 * Extensible options for a continuous scan.
 *
 * Initialize this with `paimon_stream_scan_options_init`; future versions may
 * consume fields from `reserved` while preserving this prefix.
 */
typedef struct paimon_stream_scan_options {
  uint32_t struct_size;
  int32_t startup_mode;
  int32_t follow_up_mode;
  int64_t snapshot_id;
  uint64_t reserved[4];
} paimon_stream_scan_options;

typedef struct paimon_record_batch_reader {
  void *inner;
} paimon_record_batch_reader;

typedef struct paimon_result_next_batch {
  struct paimon_arrow_batch batch;
  struct paimon_error *error;
} paimon_result_next_batch;

typedef struct paimon_stream_plan {
  void *inner;
} paimon_stream_plan;

typedef struct paimon_result_stream_poll {
  int32_t status;
  struct paimon_stream_plan *plan;
  int64_t snapshot_id;
  int64_t next_snapshot_id;
  int64_t watermark;
  uint8_t has_watermark;
  uint8_t reserved[7];
  struct paimon_error *error;
} paimon_result_stream_poll;

typedef struct paimon_result_record_batch_reader {
  struct paimon_record_batch_reader *reader;
  struct paimon_error *error;
} paimon_result_record_batch_reader;

typedef struct paimon_table_commit {
  void *inner;
} paimon_table_commit;

typedef struct paimon_result_postpone_fixed_bucket_write_builder {
  struct paimon_postpone_fixed_bucket_write_builder *write_builder;
  struct paimon_error *error;
} paimon_result_postpone_fixed_bucket_write_builder;

typedef struct paimon_result_read_builder {
  struct paimon_read_builder *read_builder;
  struct paimon_error *error;
} paimon_result_read_builder;

/**
 * Opaque wrapper around a vector-search builder.
 */
typedef struct paimon_vector_search_builder {
  void *inner;
} paimon_vector_search_builder;

typedef struct paimon_result_vector_search_builder {
  struct paimon_vector_search_builder *builder;
  struct paimon_error *error;
} paimon_result_vector_search_builder;

typedef struct paimon_write_builder {
  void *inner;
} paimon_write_builder;

typedef struct paimon_result_write_builder {
  struct paimon_write_builder *write_builder;
  struct paimon_error *error;
} paimon_result_write_builder;

typedef struct paimon_table_write {
  void *inner;
} paimon_table_write;

typedef struct paimon_result_prepare_commit {
  struct paimon_commit_messages *messages;
  struct paimon_error *error;
} paimon_result_prepare_commit;

typedef struct paimon_result_table_commit {
  struct paimon_table_commit *commit;
  struct paimon_error *error;
} paimon_result_table_commit;

typedef struct paimon_result_table_write {
  struct paimon_table_write *write;
  struct paimon_error *error;
} paimon_result_table_write;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * ABI version for the native C boundary.
 *
 * Version 1 is additive: callers must still feature-detect newer symbols when
 * loading the shared library dynamically.
 */
uint32_t paimon_abi_version(void);

/**
 * Free the ArrowArray and ArrowSchema container structs for a single batch.
 *
 * # Safety
 * `batch` must contain valid pointers returned by `paimon_record_batch_reader_next`.
 */
void paimon_arrow_batch_free(struct paimon_arrow_batch batch);

/**
 * # Safety
 * `reader` is null or was returned by `paimon_blob_reader_new`.
 */
void paimon_blob_reader_free(struct paimon_blob_reader *reader);

/**
 * # Safety
 * `options` is null for zero length or points to valid UTF-8 C-string pairs.
 */
struct paimon_result_blob_reader paimon_blob_reader_new(const struct paimon_option *options,
                                                        size_t options_len);

/**
 * Open one descriptor for incremental reads.
 *
 * # Safety
 * `reader` is valid and `descriptor` points to `descriptor_len` bytes.
 */
struct paimon_result_blob_stream paimon_blob_reader_open_blob(const struct paimon_blob_reader *reader,
                                                              const uint8_t *descriptor,
                                                              size_t descriptor_len);

/**
 * # Safety
 * The handle and input slices are valid for this call. Free the output with
 * `paimon_bytes_array_free`.
 */
struct paimon_result_read_blobs paimon_blob_reader_read_blobs(const struct paimon_blob_reader *reader,
                                                              const struct paimon_byte_slice *descriptors,
                                                              size_t descriptors_len);

/**
 * # Safety
 * `stream` is null or was returned by `paimon_blob_reader_open_blob`.
 */
void paimon_blob_stream_free(struct paimon_blob_stream *stream);

/**
 * Read at most `buffer_len` bytes into caller-owned memory.
 *
 * A zero `bytes_read` result means end of stream when `buffer_len` is nonzero.
 *
 * # Safety
 * `stream` is valid and `buffer` points to `buffer_len` writable bytes.
 */
struct paimon_result_blob_stream_read paimon_blob_stream_read(struct paimon_blob_stream *stream,
                                                              uint8_t *buffer,
                                                              size_t buffer_len);

/**
 * Seek within the descriptor's range. `whence` uses the standard 0, 1, 2 values.
 *
 * # Safety
 * `stream` is valid.
 */
struct paimon_result_blob_stream_seek paimon_blob_stream_seek(struct paimon_blob_stream *stream,
                                                              int64_t offset,
                                                              int32_t whence);

/**
 * # Safety
 * `array` was returned by `paimon_blob_reader_read_blobs`.
 */
void paimon_bytes_array_free(struct paimon_bytes_array array);

/**
 * Free a paimon_bytes buffer.
 *
 * # Safety
 * Only call with bytes returned from paimon C functions.
 */
void paimon_bytes_free(struct paimon_bytes bytes);

/**
 * Create a catalog using CatalogFactory with the given options.
 *
 * # Safety
 * `options` must be a valid pointer to an array of `paimon_option` with `options_len` elements.
 * Each key and value in the options must be valid null-terminated C strings.
 */
struct paimon_result_catalog_new paimon_catalog_create(const struct paimon_option *options,
                                                       size_t options_len);

/**
 * Create a table from a logical Paimon `Schema` JSON document.
 *
 * The input is normalized and validated through `SchemaBuilder` before it is
 * sent to the catalog. Field IDs in the JSON are therefore treated as input
 * ordering hints and reassigned canonically from zero.
 *
 * # Safety
 * `catalog` and `identifier` must be valid Paimon handles. `schema_json` must
 * point to a valid null-terminated UTF-8 string.
 */
struct paimon_error *paimon_catalog_create_table_from_schema_json(const struct paimon_catalog *catalog,
                                                                  const struct paimon_identifier *identifier,
                                                                  const char *schema_json,
                                                                  bool ignore_if_exists);

/**
 * Drop a table from the catalog.
 *
 * # Safety
 * `catalog` and `identifier` must be valid Paimon handles, or null (returns an
 * error).
 */
struct paimon_error *paimon_catalog_drop_table(const struct paimon_catalog *catalog,
                                               const struct paimon_identifier *identifier,
                                               bool ignore_if_not_exists);

/**
 * Free a paimon_catalog.
 *
 * # Safety
 * Only call with a catalog returned from `paimon_catalog_create`.
 */
void paimon_catalog_free(struct paimon_catalog *catalog);

/**
 * Get a table from the catalog.
 *
 * # Safety
 * `catalog` and `identifier` must be valid pointers from previous paimon C calls, or null (returns error).
 */
struct paimon_result_get_table paimon_catalog_get_table(const struct paimon_catalog *catalog,
                                                        const struct paimon_identifier *identifier);

/**
 * Free standard commit messages.
 */
void paimon_commit_messages_free(struct paimon_commit_messages *msgs);

/**
 * Merge standard commit messages for one logical commit.
 */
struct paimon_error *paimon_commit_messages_merge(struct paimon_commit_messages *target,
                                                  const struct paimon_commit_messages *source);

/**
 * Bind standard commit messages to a monotonically increasing streaming
 * commit identifier. The returned prepared commit owns a clone of the
 * messages, so the source handle remains valid. Valid identifiers are in
 * `[0, INT64_MAX)`; `INT64_MAX` is reserved for unidentified batch commits.
 */
struct paimon_result_prepared_commit paimon_commit_messages_prepare(const struct paimon_commit_messages *msgs,
                                                                    int64_t commit_identifier);

/**
 * Free a paimon_error.
 *
 * # Safety
 * Only call with errors returned from paimon C functions.
 */
void paimon_error_free(struct paimon_error *err);

/**
 * Create a reusable FileIO from a representative storage path and options.
 */
struct paimon_result_file_io_new paimon_file_io_create(const char *path,
                                                       const struct paimon_option *options,
                                                       size_t options_len);

/**
 * Create a reusable FileIO backed by a caller-managed block cache.
 *
 * A non-null `callbacks->get` and a non-zero `block_size` are required.
 * `whitelist` may be null to use `meta,global-index`. Once validation and
 * storage construction succeed, Rust owns `callbacks->context` and invokes
 * `destroy` exactly once after the last derived FileIO/table is dropped.
 */
struct paimon_result_file_io_new paimon_file_io_create_with_cache_v1(const char *path,
                                                                     const struct paimon_option *options,
                                                                     size_t options_len,
                                                                     const struct paimon_file_cache_callbacks_v1 *callbacks,
                                                                     uint64_t block_size,
                                                                     const char *whitelist);

/**
 * Free a FileIO handle. Tables created from it retain their own clone.
 */
void paimon_file_io_free(struct paimon_file_io *file_io);

/**
 * Free a paimon_identifier.
 *
 * # Safety
 * Only call with an identifier returned from `paimon_identifier_new`.
 */
void paimon_identifier_free(struct paimon_identifier *id);

/**
 * Create a new Identifier.
 *
 * # Safety
 * `database` and `object` must be valid null-terminated C strings, or null (returns error).
 */
struct paimon_result_identifier_new paimon_identifier_new(const char *database, const char *object);

/**
 * Return the paimon-rust package version as an owned UTF-8 byte buffer.
 *
 * The returned bytes are not NUL terminated and must be released with
 * `paimon_bytes_free`.
 */
struct paimon_bytes paimon_library_version(void);

/**
 * Free a paimon_plan.
 *
 * # Safety
 * Only call with a plan returned from `paimon_table_scan_plan`.
 * A plan returned from `paimon_plan_from_split_bytes` is also a valid source.
 */
void paimon_plan_free(struct paimon_plan *plan);

/**
 * Build a one-split `paimon_plan` from a serialized Paimon-native `DataSplit`
 * byte buffer (the wire form produced by `DataSplit::serialize` / Java
 * `DataSplit#serialize`). `data` must be raw bytes (Base64 already decoded by
 * the caller).
 *
 * The returned plan is usable with `paimon_table_read_to_arrow` and must be
 * freed with `paimon_plan_free`.
 *
 * # Safety
 * `data` must point to `len` valid bytes, or be null when `len == 0`.
 */
struct paimon_result_plan paimon_plan_from_split_bytes(const uint8_t *data, size_t len);

/**
 * Return the number of data splits in a plan.
 *
 * # Safety
 * `plan` must be a valid pointer from `paimon_table_scan_plan`, or null (returns 0).
 * A plan returned from `paimon_plan_from_split_bytes` is also a valid source.
 */
size_t paimon_plan_num_splits(const struct paimon_plan *plan);

/**
 * Free postpone fixed-bucket commit messages.
 */
void paimon_postpone_fixed_bucket_commit_messages_free(struct paimon_postpone_fixed_bucket_commit_messages *msgs);

/**
 * Merge postpone fixed-bucket messages for one logical commit.
 */
struct paimon_error *paimon_postpone_fixed_bucket_commit_messages_merge(struct paimon_postpone_fixed_bucket_commit_messages *target,
                                                                        const struct paimon_postpone_fixed_bucket_commit_messages *source);

/**
 * Abort postpone fixed-bucket commit messages.
 */
struct paimon_error *paimon_postpone_fixed_bucket_table_commit_abort(const struct paimon_postpone_fixed_bucket_table_commit *tc,
                                                                     struct paimon_postpone_fixed_bucket_commit_messages *msgs);

/**
 * Commit postpone fixed-bucket messages using the builder's mode.
 */
struct paimon_error *paimon_postpone_fixed_bucket_table_commit_commit(const struct paimon_postpone_fixed_bucket_table_commit *tc,
                                                                      struct paimon_postpone_fixed_bucket_commit_messages *msgs);

/**
 * Commit postpone fixed-bucket messages with an identifier.
 */
struct paimon_error *paimon_postpone_fixed_bucket_table_commit_commit_with_identifier(const struct paimon_postpone_fixed_bucket_table_commit *tc,
                                                                                      struct paimon_postpone_fixed_bucket_commit_messages *msgs,
                                                                                      int64_t commit_identifier);

/**
 * Filter a committed identifier before committing fixed-bucket messages.
 */
struct paimon_error *paimon_postpone_fixed_bucket_table_commit_filter_and_commit_with_identifier(const struct paimon_postpone_fixed_bucket_table_commit *tc,
                                                                                                 struct paimon_postpone_fixed_bucket_commit_messages *msgs,
                                                                                                 int64_t commit_identifier);

/**
 * Free a postpone fixed-bucket TableCommit.
 */
void paimon_postpone_fixed_bucket_table_commit_free(struct paimon_postpone_fixed_bucket_table_commit *tc);

/**
 * Truncate a table with a postpone fixed-bucket TableCommit.
 */
struct paimon_error *paimon_postpone_fixed_bucket_table_commit_truncate_table(const struct paimon_postpone_fixed_bucket_table_commit *tc);

/**
 * Truncate a table with a stable identifier.
 */
struct paimon_error *paimon_postpone_fixed_bucket_table_commit_truncate_table_with_identifier(const struct paimon_postpone_fixed_bucket_table_commit *tc,
                                                                                              int64_t commit_identifier);

/**
 * Free a postpone fixed-bucket TableWrite.
 *
 * # Safety
 * Only call with a write returned from
 * paimon_postpone_fixed_bucket_write_builder_new_write.
 */
void paimon_postpone_fixed_bucket_table_write_free(struct paimon_postpone_fixed_bucket_table_write *tw);

/**
 * Prepare postpone fixed-bucket commit messages.
 *
 * The returned handle remains owned by the caller.
 */
struct paimon_result_postpone_fixed_bucket_prepare_commit paimon_postpone_fixed_bucket_table_write_prepare_commit(struct paimon_postpone_fixed_bucket_table_write *tw);

/**
 * Write one Arrow record batch with a postpone fixed-bucket TableWrite.
 *
 * Ownership of array and schema is transferred once Arrow import starts.
 */
struct paimon_error *paimon_postpone_fixed_bucket_table_write_write_arrow_batch(struct paimon_postpone_fixed_bucket_table_write *tw,
                                                                                void *array,
                                                                                void *schema);

/**
 * Free a postpone fixed-bucket write builder.
 *
 * # Safety
 * Only call with a builder returned from
 * `paimon_table_new_postpone_fixed_bucket_write_builder`.
 */
void paimon_postpone_fixed_bucket_write_builder_free(struct paimon_postpone_fixed_bucket_write_builder *wb);

/**
 * Create a postpone fixed-bucket TableCommit.
 */
struct paimon_result_postpone_fixed_bucket_table_commit paimon_postpone_fixed_bucket_write_builder_new_commit(const struct paimon_postpone_fixed_bucket_write_builder *wb);

/**
 * Create a postpone fixed-bucket TableWrite.
 *
 * # Safety
 * wb must be a valid fixed-bucket builder, or null (returns error).
 */
struct paimon_result_postpone_fixed_bucket_table_write paimon_postpone_fixed_bucket_write_builder_new_write(const struct paimon_postpone_fixed_bucket_write_builder *wb);

/**
 * Set a shared `partition -> total_buckets` plan.
 * The caller retains ownership when pointer or builder validation fails. Once
 * Arrow import starts, this call consumes both structs even if plan validation
 * returns an error.
 *
 * # Safety
 * `wb` must be a valid postpone fixed-bucket builder. `array` and
 * `schema` must point to initialized Arrow C Data structs.
 */
struct paimon_error *paimon_postpone_fixed_bucket_write_builder_with_bucket_plan(struct paimon_postpone_fixed_bucket_write_builder *wb,
                                                                                 void *array,
                                                                                 void *schema);

/**
 * Enable overwrite mode for a postpone fixed-bucket write operation.
 *
 * # Safety
 * `wb` must be a valid fixed-bucket builder, or null (returns error).
 */
struct paimon_error *paimon_postpone_fixed_bucket_write_builder_with_overwrite(struct paimon_postpone_fixed_bucket_write_builder *wb);

/**
 * Combine two predicates with AND. Consumes both inputs.
 *
 * # Safety
 * `a` and `b` must be valid pointers from predicate functions.
 */
struct paimon_predicate *paimon_predicate_and(struct paimon_predicate *a,
                                              struct paimon_predicate *b);

/**
 * Create a BETWEEN predicate: `low <= column <= high` (inclusive, case-sensitive
 * column match).
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_between(const struct paimon_table *table,
                                                        const char *column,
                                                        struct paimon_datum low,
                                                        struct paimon_datum high);

/**
 * Create a BETWEEN predicate with configurable column-name case sensitivity.
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_between_with_case_sensitive(const struct paimon_table *table,
                                                                            const char *column,
                                                                            struct paimon_datum low,
                                                                            struct paimon_datum high,
                                                                            bool case_sensitive);

/**
 * Create a contains predicate: `column LIKE '%datum%'` (case-sensitive column match).
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_contains(const struct paimon_table *table,
                                                         const char *column,
                                                         struct paimon_datum datum);

/**
 * Create a contains predicate with configurable column-name case sensitivity.
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_contains_with_case_sensitive(const struct paimon_table *table,
                                                                             const char *column,
                                                                             struct paimon_datum datum,
                                                                             bool case_sensitive);

/**
 * Create an ends-with predicate: `column LIKE '%datum'` (case-sensitive column match).
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_ends_with(const struct paimon_table *table,
                                                          const char *column,
                                                          struct paimon_datum datum);

/**
 * Create an ends-with predicate with configurable column-name case sensitivity.
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_ends_with_with_case_sensitive(const struct paimon_table *table,
                                                                              const char *column,
                                                                              struct paimon_datum datum,
                                                                              bool case_sensitive);

/**
 * Create an equality predicate: `column = datum` (case-sensitive column match).
 *
 * For case-insensitive column matching use
 * `paimon_predicate_equal_with_case_sensitive`.
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_equal(const struct paimon_table *table,
                                                      const char *column,
                                                      struct paimon_datum datum);

/**
 * Create an equality predicate with configurable column-name case sensitivity.
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_equal_with_case_sensitive(const struct paimon_table *table,
                                                                          const char *column,
                                                                          struct paimon_datum datum,
                                                                          bool case_sensitive);

/**
 * Free a paimon_predicate.
 *
 * # Safety
 * Only call with a predicate returned from paimon predicate functions.
 */
void paimon_predicate_free(struct paimon_predicate *p);

/**
 * Create a greater-or-equal predicate: `column >= datum` (case-sensitive column match).
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_greater_or_equal(const struct paimon_table *table,
                                                                 const char *column,
                                                                 struct paimon_datum datum);

/**
 * Create a greater-or-equal predicate with configurable column-name case sensitivity.
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_greater_or_equal_with_case_sensitive(const struct paimon_table *table,
                                                                                     const char *column,
                                                                                     struct paimon_datum datum,
                                                                                     bool case_sensitive);

/**
 * Create a greater-than predicate: `column > datum` (case-sensitive column match).
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_greater_than(const struct paimon_table *table,
                                                             const char *column,
                                                             struct paimon_datum datum);

/**
 * Create a greater-than predicate with configurable column-name case sensitivity.
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_greater_than_with_case_sensitive(const struct paimon_table *table,
                                                                                 const char *column,
                                                                                 struct paimon_datum datum,
                                                                                 bool case_sensitive);

/**
 * Create an IN predicate: `column IN (datum1, datum2, ...)` (case-sensitive column match).
 *
 * # Safety
 * `table`, `column`, and `datums` must be valid pointers. `datums_len` must be the length.
 */
struct paimon_result_predicate paimon_predicate_is_in(const struct paimon_table *table,
                                                      const char *column,
                                                      const struct paimon_datum *datums,
                                                      size_t datums_len);

/**
 * Create an IN predicate with configurable column-name case sensitivity.
 *
 * # Safety
 * `table`, `column`, and `datums` must be valid pointers. `datums_len` must be the length.
 */
struct paimon_result_predicate paimon_predicate_is_in_with_case_sensitive(const struct paimon_table *table,
                                                                          const char *column,
                                                                          const struct paimon_datum *datums,
                                                                          size_t datums_len,
                                                                          bool case_sensitive);

/**
 * Create a NOT IN predicate: `column NOT IN (datum1, datum2, ...)` (case-sensitive column match).
 *
 * # Safety
 * `table`, `column`, and `datums` must be valid pointers. `datums_len` must be the length.
 */
struct paimon_result_predicate paimon_predicate_is_not_in(const struct paimon_table *table,
                                                          const char *column,
                                                          const struct paimon_datum *datums,
                                                          size_t datums_len);

/**
 * Create a NOT IN predicate with configurable column-name case sensitivity.
 *
 * # Safety
 * `table`, `column`, and `datums` must be valid pointers. `datums_len` must be the length.
 */
struct paimon_result_predicate paimon_predicate_is_not_in_with_case_sensitive(const struct paimon_table *table,
                                                                              const char *column,
                                                                              const struct paimon_datum *datums,
                                                                              size_t datums_len,
                                                                              bool case_sensitive);

/**
 * Create an IS NOT NULL predicate (case-sensitive column match).
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_is_not_null(const struct paimon_table *table,
                                                            const char *column);

/**
 * Create an IS NOT NULL predicate with configurable column-name case sensitivity.
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_is_not_null_with_case_sensitive(const struct paimon_table *table,
                                                                                const char *column,
                                                                                bool case_sensitive);

/**
 * Create an IS NULL predicate (case-sensitive column match).
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_is_null(const struct paimon_table *table,
                                                        const char *column);

/**
 * Create an IS NULL predicate with configurable column-name case sensitivity.
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_is_null_with_case_sensitive(const struct paimon_table *table,
                                                                            const char *column,
                                                                            bool case_sensitive);

/**
 * Create a less-or-equal predicate: `column <= datum` (case-sensitive column match).
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_less_or_equal(const struct paimon_table *table,
                                                              const char *column,
                                                              struct paimon_datum datum);

/**
 * Create a less-or-equal predicate with configurable column-name case sensitivity.
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_less_or_equal_with_case_sensitive(const struct paimon_table *table,
                                                                                  const char *column,
                                                                                  struct paimon_datum datum,
                                                                                  bool case_sensitive);

/**
 * Create a less-than predicate: `column < datum` (case-sensitive column match).
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_less_than(const struct paimon_table *table,
                                                          const char *column,
                                                          struct paimon_datum datum);

/**
 * Create a less-than predicate with configurable column-name case sensitivity.
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_less_than_with_case_sensitive(const struct paimon_table *table,
                                                                              const char *column,
                                                                              struct paimon_datum datum,
                                                                              bool case_sensitive);

/**
 * Create a LIKE predicate: `column LIKE pattern ESCAPE escape` (case-sensitive
 * column match). `escape == 0` uses the default escape character.
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_like(const struct paimon_table *table,
                                                     const char *column,
                                                     struct paimon_datum pattern,
                                                     char escape);

/**
 * Create a LIKE predicate with configurable column-name case sensitivity.
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_like_with_case_sensitive(const struct paimon_table *table,
                                                                         const char *column,
                                                                         struct paimon_datum pattern,
                                                                         char escape,
                                                                         bool case_sensitive);

/**
 * Negate a predicate with NOT. Consumes the input.
 *
 * # Safety
 * `p` must be a valid pointer from a predicate function.
 */
struct paimon_predicate *paimon_predicate_not(struct paimon_predicate *p);

/**
 * Create a NOT BETWEEN predicate: `column < low OR column > high`
 * (case-sensitive column match).
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_not_between(const struct paimon_table *table,
                                                            const char *column,
                                                            struct paimon_datum low,
                                                            struct paimon_datum high);

/**
 * Create a NOT BETWEEN predicate with configurable column-name case sensitivity.
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_not_between_with_case_sensitive(const struct paimon_table *table,
                                                                                const char *column,
                                                                                struct paimon_datum low,
                                                                                struct paimon_datum high,
                                                                                bool case_sensitive);

/**
 * Create a not-equal predicate: `column != datum` (case-sensitive column match).
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_not_equal(const struct paimon_table *table,
                                                          const char *column,
                                                          struct paimon_datum datum);

/**
 * Create a not-equal predicate with configurable column-name case sensitivity.
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_not_equal_with_case_sensitive(const struct paimon_table *table,
                                                                              const char *column,
                                                                              struct paimon_datum datum,
                                                                              bool case_sensitive);

/**
 * Combine two predicates with OR. Consumes both inputs.
 *
 * # Safety
 * `a` and `b` must be valid pointers from predicate functions.
 */
struct paimon_predicate *paimon_predicate_or(struct paimon_predicate *a,
                                             struct paimon_predicate *b);

/**
 * Create a starts-with predicate: `column LIKE 'datum%'` (case-sensitive column match).
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_starts_with(const struct paimon_table *table,
                                                            const char *column,
                                                            struct paimon_datum datum);

/**
 * Create a starts-with predicate with configurable column-name case sensitivity.
 *
 * # Safety
 * `table` and `column` must be valid pointers.
 */
struct paimon_result_predicate paimon_predicate_starts_with_with_case_sensitive(const struct paimon_table *table,
                                                                                const char *column,
                                                                                struct paimon_datum datum,
                                                                                bool case_sensitive);

/**
 * Restore a prepared commit serialized by `paimon_prepared_commit_serialize`.
 */
struct paimon_result_prepared_commit paimon_prepared_commit_deserialize(const uint8_t *data,
                                                                        size_t len);

/**
 * Free a prepared commit.
 */
void paimon_prepared_commit_free(struct paimon_prepared_commit *prepared);

/**
 * Return the commit identifier carried by a prepared commit, or -1 for null.
 */
int64_t paimon_prepared_commit_identifier(const struct paimon_prepared_commit *prepared);

/**
 * Merge two durable prepared commits produced by parallel writers for the
 * same table, commit user, mode and identifier.
 */
struct paimon_error *paimon_prepared_commit_merge(struct paimon_prepared_commit *target,
                                                  const struct paimon_prepared_commit *source);

/**
 * Serialize a prepared commit into a process-independent, versioned buffer.
 * The bytes must be released with `paimon_bytes_free`.
 */
struct paimon_result_bytes paimon_prepared_commit_serialize(const struct paimon_prepared_commit *prepared);

/**
 * Free a paimon_read_builder.
 *
 * # Safety
 * Only call with a read_builder returned from `paimon_table_new_read_builder`.
 */
void paimon_read_builder_free(struct paimon_read_builder *rb);

/**
 * Create a new TableRead from a ReadBuilder.
 *
 * # Safety
 * `rb` must be a valid pointer from `paimon_table_new_read_builder`, or null (returns error).
 */
struct paimon_result_new_read paimon_read_builder_new_read(const struct paimon_read_builder *rb);

/**
 * Create a new TableScan from a ReadBuilder.
 *
 * # Safety
 * `rb` must be a valid pointer from `paimon_table_new_read_builder`, or null (returns error).
 */
struct paimon_result_table_scan paimon_read_builder_new_scan(const struct paimon_read_builder *rb);

/**
 * Create an owned stream scan from a read builder.
 *
 * The returned scan clones all required Rust state and remains valid after
 * the read builder and table handles are freed. A scan handle is
 * single-thread-confined: callers must serialize poll/checkpoint/restore/free.
 */
struct paimon_result_stream_scan paimon_read_builder_new_stream_scan(const struct paimon_read_builder *read_builder,
                                                                     const struct paimon_stream_scan_options *options);

/**
 * Set whether column-name matching for **projection** is case-sensitive for
 * this ReadBuilder. Defaults to `true` (exact match). When `false`, projected
 * column names are matched by ASCII case-folding and an ambiguous
 * (case-colliding) request errors.
 *
 * This does **not** affect predicate resolution: a predicate is resolved when
 * it is constructed, so its case sensitivity is chosen by which constructor
 * you call — `paimon_predicate_*` (case-sensitive) or the additive
 * `paimon_predicate_*_with_case_sensitive` variant — independently of this
 * setting.
 *
 * # Safety
 * `rb` must be a valid pointer from `paimon_table_new_read_builder`, or null (returns error).
 */
struct paimon_error *paimon_read_builder_with_case_sensitive(struct paimon_read_builder *rb,
                                                             bool case_sensitive);

/**
 * Set a filter predicate for scan planning.
 *
 * The predicate is consumed (ownership transferred to the read builder).
 * Pass null to clear any previously set filter.
 *
 * # Safety
 * `rb` must be a valid pointer from `paimon_table_new_read_builder`, or null (returns error).
 * `predicate` must be a valid pointer from a `paimon_predicate_*` function, or null.
 */
struct paimon_error *paimon_read_builder_with_filter(struct paimon_read_builder *rb,
                                                     struct paimon_predicate *predicate);

/**
 * Set column projection for a ReadBuilder.
 *
 * The `columns` parameter is a null-terminated array of null-terminated C strings.
 * Output order follows the caller-specified order. An empty list is a valid
 * zero-column projection. An obvious typo — a name that matches no field under
 * any case sensitivity — is rejected by this call. Case-dependent resolution
 * (a name that matches only case-insensitively, or a case-fold ambiguity) is
 * deferred to `paimon_read_builder_new_read`, which uses the case sensitivity
 * effective then, so this stays order-independent with
 * `paimon_read_builder_with_case_sensitive`.
 *
 * # Safety
 * `rb` must be a valid pointer from `paimon_table_new_read_builder`, or null (returns error).
 * `columns` must be a null-terminated array of null-terminated C strings, or null for no projection.
 */
struct paimon_error *paimon_read_builder_with_projection(struct paimon_read_builder *rb,
                                                         const char *const *columns);

/**
 * Free a paimon_record_batch_reader.
 *
 * # Safety
 * Only call with a reader returned from `paimon_table_read_to_arrow` or
 * `paimon_vector_search_builder_execute_read`.
 */
void paimon_record_batch_reader_free(struct paimon_record_batch_reader *reader);

/**
 * Get the next Arrow record batch from the reader.
 *
 * When the stream is exhausted, both `batch.array` and `batch.schema` will
 * be null. On error, `error` will be non-null.
 *
 * After importing each batch, call `paimon_arrow_batch_free` to free the
 * ArrowArray and ArrowSchema container structs.
 *
 * # Safety
 * `reader` must be a valid pointer from `paimon_table_read_to_arrow`, or null (returns error).
 */
struct paimon_result_next_batch paimon_record_batch_reader_next(struct paimon_record_batch_reader *reader);

/**
 * Restore a stream plan serialized by `paimon_stream_plan_serialize`.
 */
struct paimon_result_stream_poll paimon_stream_plan_deserialize(const uint8_t *data, size_t len);

/**
 * Free a stream plan. It is valid to pass null.
 */
void paimon_stream_plan_free(struct paimon_stream_plan *plan);

/**
 * Return whether a stream plan is an initial full-snapshot plan.
 */
uint8_t paimon_stream_plan_is_full(const struct paimon_stream_plan *plan);

/**
 * Return the number of work splits in a stream plan.
 */
size_t paimon_stream_plan_num_splits(const struct paimon_stream_plan *plan);

/**
 * Read a contiguous split range from a stream plan.
 *
 * `read_mode=PAIMON_STREAM_READ_AUDIT_LOG` exposes a stable UTF-8 `rowkind`
 * column for incremental plans. Full startup plans currently support data
 * mode only; callers requiring one fixed audit schema should start at
 * `latest` or `from-snapshot`.
 */
struct paimon_result_record_batch_reader paimon_stream_plan_read_to_arrow(const struct paimon_table_read *read,
                                                                          const struct paimon_stream_plan *plan,
                                                                          size_t offset,
                                                                          size_t length,
                                                                          int32_t read_mode);

/**
 * Serialize planned-but-not-yet-consumed work for an external checkpoint.
 *
 * The current format checkpoints at plan boundaries. If rows from a plan have already
 * been exposed, callers must either replay the plan after recovery or persist
 * their own logical rows-to-skip position alongside this buffer.
 * Plans containing external data-file paths are rejected because version 1
 * recovery cannot revalidate those paths against a trusted manifest.
 */
struct paimon_result_bytes paimon_stream_plan_serialize(const struct paimon_stream_plan *plan);

/**
 * Return the next-snapshot cursor, or -1 before a startup position exists.
 */
int64_t paimon_stream_scan_checkpoint(const struct paimon_stream_scan *scan);

/**
 * Free a stream scan. It is valid to pass null.
 */
void paimon_stream_scan_free(struct paimon_stream_scan *scan);

/**
 * Fill stream options with forward-compatible defaults (`latest-full`,
 * automatic delta/changelog selection).
 */
struct paimon_error *paimon_stream_scan_options_init(struct paimon_stream_scan_options *options);

/**
 * Poll once for a snapshot plan. This call never waits for a future snapshot.
 * Calls using the same scan handle must not overlap on different threads.
 */
struct paimon_result_stream_poll paimon_stream_scan_poll(struct paimon_stream_scan *scan);

/**
 * Restore a next-snapshot cursor. Pass -1 to reapply the configured startup
 * mode; non-negative values must name a valid Paimon snapshot position.
 * This call must not overlap poll/checkpoint/free on the same handle.
 */
struct paimon_error *paimon_stream_scan_restore(struct paimon_stream_scan *scan,
                                                int64_t next_snapshot_id);

/**
 * Abort standard commit messages.
 */
struct paimon_error *paimon_table_commit_abort(const struct paimon_table_commit *tc,
                                               struct paimon_commit_messages *msgs);

/**
 * Abort files referenced by a durable prepared commit.
 *
 * Do not call this after an indeterminate commit response: retry
 * `paimon_table_commit_commit_prepared` first so a successful commit is not
 * followed by deletion of its files. The caller must also fence/serialize all
 * commit and abort operations for the same `(table, commit_user)` across
 * processes. If retained snapshot history cannot prove that abort is safe,
 * this function fails closed and deletes nothing.
 */
struct paimon_error *paimon_table_commit_abort_prepared(const struct paimon_table_commit *tc,
                                                        const struct paimon_prepared_commit *prepared);

/**
 * Commit standard append messages.
 */
struct paimon_error *paimon_table_commit_commit(const struct paimon_table_commit *tc,
                                                struct paimon_commit_messages *msgs);

/**
 * Commit a durable prepared commit using the retry-safe identifier path.
 *
 * This is the correct operation after restoring a prepared commit or after a
 * previous commit returned an indeterminate transport/IO error. A successful
 * earlier commit with the same `(commit_user, commit_identifier)` is filtered.
 */
struct paimon_error *paimon_table_commit_commit_prepared(const struct paimon_table_commit *tc,
                                                         const struct paimon_prepared_commit *prepared);

/**
 * Commit standard append messages with an identifier.
 */
struct paimon_error *paimon_table_commit_commit_with_identifier(const struct paimon_table_commit *tc,
                                                                struct paimon_commit_messages *msgs,
                                                                int64_t commit_identifier);

/**
 * Filter a committed identifier before committing standard append messages.
 */
struct paimon_error *paimon_table_commit_filter_and_commit_with_identifier(const struct paimon_table_commit *tc,
                                                                           struct paimon_commit_messages *msgs,
                                                                           int64_t commit_identifier);

/**
 * Free a standard TableCommit.
 */
void paimon_table_commit_free(struct paimon_table_commit *tc);

/**
 * Commit standard overwrite messages.
 */
struct paimon_error *paimon_table_commit_overwrite(const struct paimon_table_commit *tc,
                                                   struct paimon_commit_messages *msgs);

/**
 * Commit standard overwrite messages with an identifier.
 */
struct paimon_error *paimon_table_commit_overwrite_with_identifier(const struct paimon_table_commit *tc,
                                                                   struct paimon_commit_messages *msgs,
                                                                   int64_t commit_identifier);

/**
 * Truncate a table with a standard TableCommit.
 */
struct paimon_error *paimon_table_commit_truncate_table(const struct paimon_table_commit *tc);

/**
 * Truncate a table with a stable identifier.
 */
struct paimon_error *paimon_table_commit_truncate_table_with_identifier(const struct paimon_table_commit *tc,
                                                                        int64_t commit_identifier);

/**
 * Free a paimon_table.
 *
 * # Safety
 * Only call with a table returned from `paimon_catalog_get_table`,
 * `paimon_table_from_schema_json`, or
 * `paimon_table_from_schema_json_with_file_io`.
 */
void paimon_table_free(struct paimon_table *table);

/**
 * Create a table directly from a resolved Paimon table schema JSON.
 *
 * This constructor does not create a catalog or derive a warehouse. Storage
 * options are used only to build FileIO; they are not merged into the supplied
 * table schema. `branch` selects the branch-scoped managers while preserving
 * the supplied schema; pass null to default to the `main` branch.
 *
 * # Safety
 * All string pointers except `branch` must be valid null-terminated C strings.
 * `branch` may be null to select the default `main` branch, or a valid
 * null-terminated C string. `storage_options` must point to
 * `storage_options_len` valid `paimon_option` values, or be null when
 * `storage_options_len` is 0.
 */
struct paimon_result_get_table paimon_table_from_schema_json(const char *table_path,
                                                             const char *table_schema_json,
                                                             const char *database,
                                                             const char *table_name,
                                                             const char *branch,
                                                             const struct paimon_option *storage_options,
                                                             size_t storage_options_len);

/**
 * Create a table from a resolved schema and a caller-created FileIO.
 *
 * The FileIO is cloned into the table, so its handle may be freed immediately
 * after this call. This additive API allows native embedders to share storage
 * and an externally managed cache across tables.
 *
 * # Safety
 * `file_io` must be returned by a Paimon FileIO constructor. All string
 * pointers except `branch` must be valid null-terminated C strings. `branch`
 * may be null to select `main`.
 */
struct paimon_result_get_table paimon_table_from_schema_json_with_file_io(const struct paimon_file_io *file_io,
                                                                          const char *table_path,
                                                                          const char *table_schema_json,
                                                                          const char *database,
                                                                          const char *table_name,
                                                                          const char *branch);

/**
 * Create a reader using a table's FileIO.
 *
 * # Safety
 * `table` is a valid handle returned by the Paimon C API.
 */
struct paimon_result_blob_reader paimon_table_new_blob_reader(const struct paimon_table *table);

/**
 * Create a one-shot fixed-bucket WriteBuilder for a postpone table.
 * A bucket plan must be set before creating a writer.
 *
 * # Safety
 * `table` must be a valid table pointer, or null (returns error).
 */
struct paimon_result_postpone_fixed_bucket_write_builder paimon_table_new_postpone_fixed_bucket_write_builder(const struct paimon_table *table);

/**
 * Create a fixed-bucket WriteBuilder with a stable commit identity.
 * A bucket plan must be set before creating a writer.
 *
 * # Safety
 * `table` must be a valid table pointer. `commit_user` must be a valid UTF-8
 * C string and a safe file-name segment.
 */
struct paimon_result_postpone_fixed_bucket_write_builder paimon_table_new_postpone_fixed_bucket_write_builder_with_commit_user(const struct paimon_table *table,
                                                                                                                               const char *commit_user);

/**
 * Create a new ReadBuilder from a Table.
 *
 * # Safety
 * `table` must be a valid pointer from `paimon_catalog_get_table` or
 * `paimon_table_from_schema_json`, or null (returns error).
 */
struct paimon_result_read_builder paimon_table_new_read_builder(const struct paimon_table *table);

/**
 * Create a ReadBuilder from a Table with scan options (e.g. time-travel
 * selectors `scan.snapshot-id` / `scan.tag-name` / `scan.timestamp-millis` /
 * `scan.watermark` / `scan.version`). At most one time-travel selector may be
 * set. A selector that does not resolve to a snapshot is an error (never a
 * silent read-of-latest).
 *
 * # Safety
 * `table` must be a valid pointer. `options` must be a valid pointer to
 * `options_len` `paimon_option` values, or null when `options_len` is 0.
 */
struct paimon_result_read_builder paimon_table_new_read_builder_with_options(const struct paimon_table *table,
                                                                             const struct paimon_option *options,
                                                                             size_t options_len);

/**
 * Create a new vector-search builder from a Table.
 *
 * # Safety
 * `table` must be a valid pointer from `paimon_catalog_get_table` or
 * `paimon_table_from_schema_json`, or null (returns error).
 */
struct paimon_result_vector_search_builder paimon_table_new_vector_search_builder(const struct paimon_table *table);

/**
 * Create a new WriteBuilder from a Table.
 *
 * The returned WriteBuilder holds a shared `commit_user` (UUID) that will be
 * used by both `new_write()` and `new_commit()` for duplicate-commit detection.
 *
 * # Safety
 * `table` must be a valid pointer from `paimon_catalog_get_table` or
 * `paimon_table_from_schema_json`, or null (returns error).
 */
struct paimon_result_write_builder paimon_table_new_write_builder(const struct paimon_table *table);

/**
 * Create a WriteBuilder with a caller-provided stable commit identity.
 *
 * Writers whose messages are merged into one logical commit must use the
 * same `commit_user`.
 *
 * # Safety
 * `table` must be a valid table pointer. `commit_user` must be a valid UTF-8
 * C string and a safe file-name segment.
 */
struct paimon_result_write_builder paimon_table_new_write_builder_with_commit_user(const struct paimon_table *table,
                                                                                   const char *commit_user);

/**
 * Free a paimon_table_read.
 *
 * # Safety
 * Only call with a read returned from `paimon_read_builder_new_read`.
 */
void paimon_table_read_free(struct paimon_table_read *read);

/**
 * Read table data as Arrow record batches via a streaming reader.
 *
 * Returns a `paimon_record_batch_reader` that yields one batch at a time
 * via `paimon_record_batch_reader_next`. This avoids loading all batches
 * into memory at once.
 *
 * `offset` and `length` select a contiguous sub-range of splits from the
 * plan. The range is clamped to the available splits (out-of-range values
 * are silently adjusted).
 *
 * # Safety
 * `read` and `plan` must be valid pointers from previous paimon C calls, or null (returns error).
 */
struct paimon_result_record_batch_reader paimon_table_read_to_arrow(const struct paimon_table_read *read,
                                                                    const struct paimon_plan *plan,
                                                                    size_t offset,
                                                                    size_t length);

/**
 * Free a paimon_table_scan.
 *
 * # Safety
 * Only call with a scan returned from `paimon_read_builder_new_scan`.
 */
void paimon_table_scan_free(struct paimon_table_scan *scan);

/**
 * Execute a scan plan to get splits.
 *
 * # Safety
 * `scan` must be a valid pointer from `paimon_read_builder_new_scan`, or null (returns error).
 */
struct paimon_result_plan paimon_table_scan_plan(const struct paimon_table_scan *scan);

/**
 * Free a standard TableWrite.
 *
 * # Safety
 * Only call with a write returned from paimon_write_builder_new_write.
 */
void paimon_table_write_free(struct paimon_table_write *tw);

/**
 * Prepare standard commit messages.
 *
 * The returned handle remains owned by the caller.
 */
struct paimon_result_prepare_commit paimon_table_write_prepare_commit(struct paimon_table_write *tw);

/**
 * Write one Arrow record batch with a standard TableWrite.
 *
 * Ownership of array and schema is transferred once Arrow import starts.
 */
struct paimon_error *paimon_table_write_write_arrow_batch(struct paimon_table_write *tw,
                                                          void *array,
                                                          void *schema);

/**
 * Execute the vector search and return a streaming Arrow reader over the
 * materialized rows (projected user columns plus `__paimon_search_score`).
 * Works for both primary-key and data-evolution tables. Consume via
 * `paimon_record_batch_reader_next` and free with `paimon_record_batch_reader_free`.
 *
 * # Safety
 * `b` must be a valid pointer from `paimon_table_new_vector_search_builder`, or
 * null (returns an error result).
 */
struct paimon_result_record_batch_reader paimon_vector_search_builder_execute_read(struct paimon_vector_search_builder *b);

/**
 * Free a paimon_vector_search_builder.
 *
 * # Safety
 * Only call with a builder returned from `paimon_table_new_vector_search_builder`.
 */
void paimon_vector_search_builder_free(struct paimon_vector_search_builder *b);

/**
 * Set an optional scalar residual filter for a vector-search builder.
 *
 * The predicate is consumed (ownership transferred to the builder). Pass null
 * to clear any previously set filter.
 *
 * # Safety
 * `b` must be a valid pointer from `paimon_table_new_vector_search_builder`, or
 * null (returns error). `predicate` must be a valid pointer from a
 * `paimon_predicate_*` function, or null.
 */
struct paimon_error *paimon_vector_search_builder_with_filter(struct paimon_vector_search_builder *b,
                                                              struct paimon_predicate *predicate);

/**
 * Set the maximum number of results for a vector-search builder.
 *
 * # Safety
 * `b` must be a valid pointer from `paimon_table_new_vector_search_builder`, or
 * null (returns error).
 */
struct paimon_error *paimon_vector_search_builder_with_limit(struct paimon_vector_search_builder *b,
                                                             size_t limit);

/**
 * Set scan/search options for a vector-search builder.
 *
 * # Safety
 * `b` must be a valid pointer from `paimon_table_new_vector_search_builder`, or
 * null (returns error). `options` must be a valid pointer to `len`
 * `paimon_option` values, or null when `len` is 0.
 */
struct paimon_error *paimon_vector_search_builder_with_options(struct paimon_vector_search_builder *b,
                                                               const struct paimon_option *options,
                                                               size_t len);

/**
 * Restrict the columns materialized by `paimon_vector_search_builder_execute_read`
 * to `columns` (plus the always-appended `__paimon_search_score`). Without this
 * call `execute_read` materializes every user table column. Only affects
 * `execute_read`.
 *
 * `columns` is a null-terminated array of null-terminated C strings; output
 * order follows the caller-specified order. An empty list is a valid zero-column
 * projection (only the score column is materialized). Pass null to clear any
 * previously set projection.
 *
 * Unlike `paimon_read_builder_with_projection`, this does not validate column
 * names eagerly: the vector builder resolves the projection against the schema
 * when the search runs, so an unknown column surfaces as an error from
 * `paimon_vector_search_builder_execute_read`.
 *
 * # Safety
 * `b` must be a valid pointer from `paimon_table_new_vector_search_builder`, or
 * null (returns error). `columns` must be a null-terminated array of
 * null-terminated C strings, or null to clear the projection.
 */
struct paimon_error *paimon_vector_search_builder_with_projection(struct paimon_vector_search_builder *b,
                                                                  const char *const *columns);

/**
 * Set the query vector for a vector-search builder.
 *
 * The `len` floats at `data` are copied into the builder; the caller retains
 * ownership of `data`. An empty vector (`len == 0`) is rejected.
 *
 * # Safety
 * `b` must be a valid pointer from `paimon_table_new_vector_search_builder`, or
 * null (returns error). `data` must point to `len` `f32` values when `len > 0`.
 */
struct paimon_error *paimon_vector_search_builder_with_query_vector(struct paimon_vector_search_builder *b,
                                                                    const float *data,
                                                                    size_t len);

/**
 * Set the target vector column for a vector-search builder.
 *
 * # Safety
 * `b` must be a valid pointer from `paimon_table_new_vector_search_builder`, or
 * null (returns error). `column` must be a valid C string.
 */
struct paimon_error *paimon_vector_search_builder_with_vector_column(struct paimon_vector_search_builder *b,
                                                                     const char *column);

/**
 * Free a paimon_write_builder.
 *
 * # Safety
 * Only call with a write_builder returned from `paimon_table_new_write_builder`.
 */
void paimon_write_builder_free(struct paimon_write_builder *wb);

/**
 * Create a standard TableCommit from a standard WriteBuilder.
 */
struct paimon_result_table_commit paimon_write_builder_new_commit(const struct paimon_write_builder *wb);

/**
 * Create a standard TableWrite from a standard WriteBuilder.
 *
 * # Safety
 * wb must be a valid standard builder, or null (returns error).
 */
struct paimon_result_table_write paimon_write_builder_new_write(const struct paimon_write_builder *wb);

/**
 * Enable overwrite mode for the WriteBuilder.
 *
 * # Safety
 * `wb` must be a valid pointer from `paimon_table_new_write_builder`, or null (returns error).
 */
struct paimon_error *paimon_write_builder_with_overwrite(struct paimon_write_builder *wb);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  /* PAIMON_C_H */
