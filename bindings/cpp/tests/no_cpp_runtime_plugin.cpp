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

#include <paimon/paimon.hpp>

#include "paimon_cpp_helper_config.h"

#ifndef PAIMON_CPP_HELPER_COMPILE_DEFINITION
#error "no-runtime helper did not forward compile definitions"
#endif

static_assert(PAIMON_CPP_HELPER_COMPILE_DEFINITION ==
                  PAIMON_CPP_HELPER_CONFIG_VALUE,
              "no-runtime helper configuration mismatch");

// These C-linkage exports make this a realistic C++ implementation plugin that
// can itself be loaded on a host without libstdc++ or libc++.
PAIMON_CPP_PLUGIN_EXPORT std::uint32_t
paimon_cpp_plugin_abi_version() noexcept {
  return paimon::abi_version();
}

PAIMON_CPP_PLUGIN_EXPORT std::size_t paimon_cpp_plugin_library_version(
    char* output, std::size_t capacity) noexcept {
  auto version = paimon::library_version();
  const auto copied = output == nullptr
                          ? 0
                          : (version.size() < capacity ? version.size()
                                                       : capacity);
  for (std::size_t index = 0; index < copied; ++index) {
    output[index] = static_cast<char>(version.data()[index]);
  }
  return version.size();
}

PAIMON_CPP_PLUGIN_EXPORT std::int32_t paimon_cpp_plugin_open_catalog(
    const paimon::Option* options, std::size_t options_len) noexcept {
  auto catalog = paimon::Catalog::create(options, options_len);
  if (!catalog) {
    return static_cast<std::int32_t>(catalog.error().code()) + 1;
  }
  // The move-only Catalog is deliberately closed by its noexcept destructor.
  return 0;
}

PAIMON_CPP_PLUGIN_EXPORT std::int32_t
paimon_cpp_plugin_error_self_reset() noexcept {
  paimon::Error error(
      paimon::adopt_handle,
      ::paimon_stream_scan_restore(nullptr, 0));
  if (!error) {
    return -1;
  }
  auto* same = error.native_handle();
  error.reset(same);
  return error.native_handle() == same ? 0 : -2;
}
