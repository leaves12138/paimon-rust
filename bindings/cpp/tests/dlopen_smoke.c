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

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint32_t (*abi_version_fn)(void);
typedef int32_t (*self_reset_fn)(void);

static void *required_symbol(void *library, const char *name) {
    void *symbol;
    dlerror();
    symbol = dlsym(library, name);
    if (symbol == NULL || dlerror() != NULL) {
        fprintf(stderr, "missing plugin symbol: %s\n", name);
        return NULL;
    }
    return symbol;
}

int main(int argc, char **argv) {
    void *library;
    void *symbol;
    abi_version_fn abi_version;
    self_reset_fn self_reset;
    if (argc != 2) {
        return 2;
    }
    library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (library == NULL) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 1;
    }
    symbol = required_symbol(library, "paimon_cpp_plugin_abi_version");
    if (symbol == NULL) {
        return 1;
    }
    memcpy(&abi_version, &symbol, sizeof(abi_version));
    symbol = required_symbol(library, "paimon_cpp_plugin_error_self_reset");
    if (symbol == NULL) {
        return 1;
    }
    memcpy(&self_reset, &symbol, sizeof(self_reset));
    if (abi_version() != 1 || self_reset() != 0) {
        return 1;
    }
    return dlclose(library) == 0 ? 0 : 1;
}
