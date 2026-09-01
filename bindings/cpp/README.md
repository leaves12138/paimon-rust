<!--
Licensed to the Apache Software Foundation (ASF) under one
or more contributor license agreements.  See the NOTICE file
distributed with this work for additional information
regarding copyright ownership.  The ASF licenses this file
to you under the Apache License, Version 2.0 (the
"License"); you may not use this file except in compliance
with the License.  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied.  See the License for the
specific language governing permissions and limitations
under the License.
-->

# Paimon C++ facade

This directory provides a header-only C++17 RAII facade over the stable Paimon
C ABI. It deliberately builds no C++ shared library: the only Paimon binary is
`libpaimon_c`, produced by Rust, and every symbol called by the facade has C
linkage. The facade does not depend on Arrow C++; Arrow batches cross the API as
raw Arrow C Data `array` and `schema` pointers.

All native handles are move-only. Their destructors are `noexcept` and only
release resources. In particular, destroying `PreparedMessages` or
`PreparedCommit` never commits or aborts them. A streaming writer binds messages
to a checkpoint with `PreparedMessages::prepare`, persists the bytes from
`PreparedCommit::serialize`, then calls `TableCommit::commit_prepared`. After an
uncertain result or process restart, deserialize the same bytes and retry with
the same stable `commit_user`; the identifier path filters duplicate commits.
Exactly-once filtering is recorded in retained snapshot metadata. Keep snapshot
history for at least the maximum writer-recovery horizon, never retry a
checkpoint older than that horizon, and give each fresh job a new globally
unique `commit_user`. Checkpoint identifiers must be in `[0, INT64_MAX)`;
`INT64_MAX` is reserved for unidentified batch commits.
Checkpoint blobs are trusted state, not a security token: store them behind
normal integrity/access controls. Before `abort_prepared`, fence every commit
and abort for the same `(table, commit_user)` across processes. If snapshot
history is too old to prove safety, abort fails closed and leaves cleanup to an
orphan-file policy.
Filesystem-catalog commits require a backend with atomic publish-if-absent
(conditional rename/copy/write). Unsupported backends fail closed; use REST
commit or an external lock instead of relying on a racy existence check.

Continuous reading is a pull API. `StreamScan::poll` immediately returns data,
waiting, or end; it never starts a callback thread and never waits for a future
snapshot. A data result owns a `StreamPlan`, which can be read in data or audit
log mode using the same Arrow C Data `RecordBatchReader` as bounded reads.
Each `StreamScan` is single-thread-confined; serialize poll, checkpoint,
restore, and destruction. Decoupled changelog fallback and consumer-retention
registration are not implemented yet, so snapshot retention must cover the
maximum expected reader lag.
Persisted stream plans currently reject external data-file paths. The failure
is reported by `StreamPlan::serialize` before a checkpoint can be acknowledged,
instead of producing a checkpoint that cannot be restored.

Catalog DDL is available directly from the facade. Creation accepts the JSON
form of Paimon's logical `Schema`; it validates and canonically reassigns field
IDs before calling the catalog. Both operations return `Status`, so callers can
choose strict or idempotent create/drop semantics without a Java helper:

```cpp
auto identifier = paimon::Identifier::create("default", "events");
auto created = catalog.create_table_from_schema_json(
    identifier.value(), schema_json, /*ignore_if_exists=*/false);
auto dropped = catalog.drop_table(
    identifier.value(), /*ignore_if_not_exists=*/true);
```

## Build

Configure and build the C++ facade directly. The build always compiles the
in-tree `bindings/c` crate first, so the C and C++ layers come from the same
source revision:

```bash
cmake -S bindings/cpp -B target/cpp-build \
  -DPAIMON_CPP_BUILD_EXAMPLES=ON
cmake --build target/cpp-build
```

The CMake build directory itself is a complete, directly consumable artifact
tree:

```text
target/cpp-build/
├── include/paimon.h
├── include/paimon/paimon.hpp
├── <libdir>/libpaimon_c.so       # Linux
└── <libdir>/cmake/PaimonCpp/
```

`<libdir>` follows GNUInstallDirs and is normally `lib` or `lib64`. macOS uses
`libpaimon_c.dylib` in the same location. The facade is header-only, so there is
intentionally no separate `libpaimon_cpp` shared library.

All platforms use `cargo build --locked --release -p paimon-c`. Build Linux
release artifacts on the oldest glibc version that must be supported; glibc is
backward compatible with binaries built against older symbol versions.
External prebuilt paimon-c libraries and parent-provided `Paimon::c` targets
are deliberately unsupported.

When changing the C ABI, regenerate the checked header separately with
`cbindgen`; ordinary builds consume the checked-in `bindings/c/include/paimon.h`.

For a shared plugin that must load without `libstdc++` or `libc++`, compile the
C++ source without exceptions/RTTI and use the C linker driver for the final
link:

```bash
c++ -std=c++17 -fPIC -fno-exceptions -fno-rtti \
  -Ibindings/cpp/include -Ibindings/c/include \
  -c plugin.cpp -o plugin.o
cc -shared plugin.o -Ltarget/release -lpaimon_c -Wl,-z,defs \
  -o libplugin.so
bindings/cpp/scripts/verify_linux_elf.sh libplugin.so
```

The plugin must expose each public entry point with
`PAIMON_CPP_PLUGIN_EXPORT` and stay within the facade's allocation-free,
no-exceptions subset. Linking the final `.so` with a C++ driver can add a C++
runtime even when the source does not call that runtime directly.

Install its CMake interface target:

```bash
cmake --install target/cpp-build --prefix /your/prefix
```

Installation always bundles the just-built `libpaimon_c` and exports imported
target `Paimon::c`. Installed consumers can build a verified no-runtime plugin
with the provided helper:

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyPaimonPlugin LANGUAGES C CXX)
find_package(PaimonCpp CONFIG REQUIRED)
paimon_add_no_runtime_plugin(
  my_paimon_plugin
  SOURCES plugin.cpp
  INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
  COMPILE_DEFINITIONS MY_PLUGIN_ABI=1)
```

Configure C++ compilation through the helper's `SOURCES`,
`INCLUDE_DIRECTORIES`, `COMPILE_DEFINITIONS`, `COMPILE_OPTIONS`, and
`LINK_LIBRARIES` arguments. Do not add C++ sources to the returned C-link
target with `target_sources`; doing so bypasses the split compile/link model.
The helper hides all non-exported C++ symbols, links with the C driver, embeds
only `$ORIGIN` as its runtime search path, and runs the installed ELF guard
after every successful link.

`Paimon::cpp` remains the header-only facade target for consumers that manage
their own final link. The installed package always resolves `Paimon::c` to the
library bundled in the same installation prefix.

## Linux runtime guard

Run the ELF guard on the library staged by CMake when validating a Linux
release artifact:

```bash
bindings/cpp/scripts/verify_linux_elf.sh \
  target/cpp-build/lib/libpaimon_c.so
```

Some distributions use `lib64` instead of `lib`. The build host determines the
minimum glibc version; build on glibc 2.17 when 2.17 is the deployment baseline.

It prints the build host's `ldd --version` and applies a `DT_NEEDED` allowlist
containing glibc components, `libgcc_s`, and `libpaimon_c`. It rejects C++
runtimes, `libunwind`, `libatomic`, `GLIBCXX`/`CXXABI` symbol versions,
undefined or exported C++ mangled symbols, unversioned host hooks, operator
new/delete, RTTI/dynamic-cast support, absolute runtime paths, and private
glibc ABI versions. Glibc's C-level `__cxa_atexit`, `__cxa_finalize`, and
`__cxa_thread_atexit_impl` remain allowed.

`Scan::plan()` remains a bounded scan. Use `StreamScanOptions` and
`ReadBuilder::new_stream_scan` for a stateful continuous scan. Persist
`StreamScan::checkpoint()` only after every split in the returned plan has been
durably accounted for by the surrounding checkpoint barrier.
