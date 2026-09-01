#!/usr/bin/env sh
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements. See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to you under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_root=$(CDPATH= cd -- "${script_dir}/../../.." && pwd)
elf_verifier="${repository_root}/bindings/cpp/scripts/verify_linux_elf.sh"

if [ -n "${PAIMON_LINUX_RUST_TARGET:-}" ]; then
  case "${PAIMON_LINUX_RUST_TARGET}" in
    x86_64-unknown-linux-gnu|aarch64-unknown-linux-gnu)
      rust_target=${PAIMON_LINUX_RUST_TARGET}
      ;;
    *)
      echo "unsupported Linux Rust target: ${PAIMON_LINUX_RUST_TARGET}" >&2
      exit 2
      ;;
  esac
else
  case "$(uname -m)" in
    x86_64|amd64)
      rust_target=x86_64-unknown-linux-gnu
      ;;
    aarch64|arm64)
      rust_target=aarch64-unknown-linux-gnu
      ;;
    *)
      echo "unsupported Linux architecture: $(uname -m)" >&2
      exit 2
      ;;
  esac
fi

if ! command -v cargo-zigbuild >/dev/null 2>&1; then
  echo "cargo-zigbuild is required to build the glibc 2.17 artifact" >&2
  exit 2
fi

cd "${repository_root}"
cargo zigbuild --locked --release -p paimon-c \
  --target "${rust_target}.2.17"

target_dir=${CARGO_TARGET_DIR:-${repository_root}/target}
case "${target_dir}" in
  /*) ;;
  *) target_dir="${repository_root}/${target_dir}" ;;
esac
library="${target_dir}/${rust_target}/release/libpaimon_c.so"
"${elf_verifier}" "${library}"
printf 'validated-library=%s\n' "${library}"
