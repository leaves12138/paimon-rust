#!/usr/bin/env sh
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

set -eu

if [ "$#" -ne 1 ]; then
  echo "usage: $0 /path/to/libpaimon_c.so" >&2
  exit 2
fi

library=$1
if [ ! -f "$library" ]; then
  echo "not a file: $library" >&2
  exit 2
fi

if command -v readelf >/dev/null 2>&1; then
  readelf_cmd=readelf
elif command -v llvm-readelf >/dev/null 2>&1; then
  readelf_cmd=llvm-readelf
else
  echo "readelf or llvm-readelf is required" >&2
  exit 2
fi

elf_header=$($readelf_cmd -h "$library")
if ! printf '%s\n' "$elf_header" | grep -q 'ELF'; then
  echo "not an ELF shared object: $library" >&2
  exit 1
fi
if ! printf '%s\n' "$elf_header" | grep -Eq 'Type:[[:space:]]+DYN'; then
  echo "ELF artifact is not a shared object: $library" >&2
  exit 1
fi

program_headers=$($readelf_cmd -W -l "$library")
if printf '%s\n' "$program_headers" | grep -q 'INTERP'; then
  echo "ELF artifact is a PIE executable, not a shared object" >&2
  printf '%s\n' "$program_headers" | grep 'INTERP' >&2
  exit 1
fi
if printf '%s\n' "$program_headers" | grep -Eq \
  'GNU_STACK.*W.*E|GNU_STACK.*E.*W'; then
  echo "forbidden executable GNU_STACK segment" >&2
  printf '%s\n' "$program_headers" | grep 'GNU_STACK' >&2
  exit 1
fi
if printf '%s\n' "$program_headers" | grep -Eq \
  'LOAD.*W.*E|LOAD.*E.*W'; then
  echo "forbidden writable and executable LOAD segment" >&2
  printf '%s\n' "$program_headers" | grep 'LOAD' >&2
  exit 1
fi

dynamic_section=$($readelf_cmd -d "$library")
if printf '%s\n' "$dynamic_section" | grep -q 'TEXTREL'; then
  echo "forbidden text relocation" >&2
  printf '%s\n' "$dynamic_section" | grep 'TEXTREL' >&2
  exit 1
fi

if command -v ldd >/dev/null 2>&1; then
  ldd --version 2>&1 | sed -n '1,2p'
fi

needed=$(printf '%s\n' "$dynamic_section" | grep 'NEEDED' || true)
printf '%s\n' "$needed"

runtime_paths=$(printf '%s\n' "$dynamic_section" |
  sed -n 's/.*(RPATH).*Library rpath: \[\([^]]*\)\].*/\1/p;
          s/.*(RUNPATH).*Library runpath: \[\([^]]*\)\].*/\1/p')
old_ifs=$IFS
IFS=:
for runtime_path in $runtime_paths; do
  case "$runtime_path" in
    '$ORIGIN'|'${ORIGIN}')
      ;;
    *)
      echo "forbidden runtime search path: $runtime_path" >&2
      exit 1
      ;;
  esac
done
IFS=$old_ifs

needed_names=$(printf '%s\n' "$needed" |
  sed -n 's/.*Shared library: \[\([^]]*\)\].*/\1/p')
for dependency in $needed_names; do
  case "$dependency" in
    libstdc++*|libc++*|libsupc++*|libgcc_s*|libunwind*|libatomic*)
      echo "forbidden non-C runtime dependency in DT_NEEDED: $dependency" >&2
      exit 1
      ;;
    libc.so.*|libm.so.*|libpthread.so.*|libdl.so.*|librt.so.*|libutil.so.*|libresolv.so.*|libanl.so.*|libBrokenLocale.so.*|libcrypt.so.*|libnss_*.so.*|ld-linux*.so.*|ld64.so.*|ld.so.*|libpaimon_c.so*)
      ;;
    *)
      echo "dependency is outside the glibc/libpaimon_c allowlist: $dependency" >&2
      exit 1
      ;;
  esac
done

undefined=$($readelf_cmd --dyn-syms --wide "$library" | grep ' UND ' || true)
unexpected_unversioned=$(printf '%s\n' "$undefined" | awk '
  {
    bind = $5
    name = $8
    base = name
    sub(/@.*/, "", base)
    if (base == "" || name ~ /@GLIBC_[0-9]/ || base ~ /^paimon_/ ||
        base ~ /^_Z/) {
      next
    }
    if (bind == "WEAK" &&
        (base == "_ITM_deregisterTMCloneTable" ||
         base == "_ITM_registerTMCloneTable" ||
         base == "__gmon_start__" ||
         base == "_Jv_RegisterClasses" ||
         base == "ZSTD_trace_compress_begin" ||
         base == "ZSTD_trace_compress_end" ||
         base == "ZSTD_trace_decompress_begin" ||
         base == "ZSTD_trace_decompress_end" ||
         base == "OPENSSL_memory_alloc" ||
         base == "OPENSSL_memory_free" ||
         base == "OPENSSL_memory_get_size" ||
         base == "OPENSSL_memory_realloc" ||
         base == "sdallocx" ||
         base == "gettid" ||
         base == "statx" ||
         base == "getrandom" ||
         base == "copy_file_range" ||
         base == "__cxa_thread_atexit_impl")) {
      next
    }
    print
  }')
if [ -n "$unexpected_unversioned" ]; then
  echo "forbidden unversioned undefined symbol; only paimon_* and narrow weak CRT hooks are allowed" >&2
  printf '%s\n' "$unexpected_unversioned" >&2
  exit 1
fi

defined=$($readelf_cmd --dyn-syms --wide "$library" |
  awk '$7 != "UND" && $8 != "" { print }')
if printf '%s\n' "$defined" | grep -Eq \
  '[[:space:]]_Z[A-Za-z0-9_$.@]*'; then
  echo "forbidden exported C++ mangled symbol" >&2
  printf '%s\n' "$defined" |
    grep -E '[[:space:]]_Z[A-Za-z0-9_$.@]*' >&2
  exit 1
fi

version_info=$($readelf_cmd --version-info --wide "$library" || true)
symbol_versions=$(printf '%s\n%s\n' "$undefined" "$version_info")
if printf '%s\n' "$symbol_versions" | grep -Eq 'GLIBCXX_|CXXABI_|GCC_[0-9]'; then
  echo "forbidden C++/compiler runtime symbol version" >&2
  printf '%s\n' "$symbol_versions" |
    grep -E 'GLIBCXX_|CXXABI_|GCC_[0-9]' >&2
  exit 1
fi

if printf '%s\n' "$undefined" | grep -Eq \
  '[[:space:]]_Z[A-Za-z0-9_$.@]*'; then
  echo "forbidden C++ mangled undefined symbol" >&2
  printf '%s\n' "$undefined" |
    grep -E '[[:space:]]_Z[A-Za-z0-9_$.@]*' >&2
  exit 1
fi

cxa_symbols=$(printf '%s\n' "$undefined" |
  grep '__cxa_' |
  grep -Ev '__cxa_(atexit|finalize|thread_atexit_impl)(@|$)' || true)
if [ -n "$cxa_symbols" ] ||
  printf '%s\n' "$undefined" | grep -Eq '__gxx_personality_v0|__dynamic_cast'; then
  echo "forbidden C++ ABI undefined symbol" >&2
  printf '%s\n' "$cxa_symbols" >&2
  printf '%s\n' "$undefined" |
    grep -E '__gxx_personality_v0|__dynamic_cast' >&2 || true
  exit 1
fi

if printf '%s\n' "$symbol_versions" | grep -Eq 'GLIBC_(PRIVATE|ABI_)'; then
  echo "private or non-baseline glibc ABI requirement" >&2
  printf '%s\n' "$symbol_versions" | grep -E 'GLIBC_(PRIVATE|ABI_)' >&2
  exit 1
fi

max_glibc=$(printf '%s\n' "$symbol_versions" |
  grep -Eo 'GLIBC_[0-9][0-9.]*' |
  sed 's/^GLIBC_//' |
  sort -V |
  tail -n 1 || true)
if [ -n "$max_glibc" ]; then
  newest=$(printf '%s\n' 2.17 "$max_glibc" | sort -V | tail -n 1)
  if [ "$newest" != "2.17" ]; then
    echo "GLIBC symbol version $max_glibc exceeds supported baseline 2.17" >&2
    printf '%s\n' "$symbol_versions" | grep "GLIBC_$max_glibc" >&2
    exit 1
  fi
fi

if ! command -v c++filt >/dev/null 2>&1; then
  echo "c++filt is required to inspect demangled undefined symbols" >&2
  exit 2
fi

demangled=$(printf '%s\n' "$undefined" | c++filt)
if printf '%s\n' "$demangled" | grep -Eq \
  'std::|__gnu_cxx::|typeinfo for|vtable for|operator (new|delete)(\[\])?\(|__dynamic_cast'; then
  echo "forbidden demangled C++ undefined symbol" >&2
  printf '%s\n' "$demangled" | grep -E \
    'std::|__gnu_cxx::|typeinfo for|vtable for|operator (new|delete)(\[\])?\(|__dynamic_cast' >&2
  exit 1
fi

echo "ELF C++ runtime check passed: $library"
