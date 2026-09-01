#!/usr/bin/env sh
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
binding_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
repo_dir=$(CDPATH= cd -- "$binding_dir/../.." && pwd)
generated=$(mktemp)
trap 'rm -f "$generated"' EXIT HUP INT TERM

cbindgen --quiet --config "$binding_dir/cbindgen.toml" \
  "$binding_dir" --output "$generated"

if ! cmp -s "$generated" "$binding_dir/include/paimon.h"; then
  echo "bindings/c/include/paimon.h is stale; regenerate it with cbindgen" >&2
  diff -u "$binding_dir/include/paimon.h" "$generated" || true
  exit 1
fi

cc -std=c11 -fsyntax-only -x c "$generated"
c++ -std=c++17 -fno-exceptions -fno-rtti -fsyntax-only -x c++ "$generated"

echo "C header is current and C/C++ compatible in $repo_dir"
