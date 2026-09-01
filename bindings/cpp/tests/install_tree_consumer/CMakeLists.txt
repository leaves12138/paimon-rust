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

cmake_minimum_required(VERSION 3.15)
project(PaimonCppInstallTreeConsumer LANGUAGES C CXX)

find_package(PaimonCpp CONFIG REQUIRED)
# Package discovery may happen through more than one dependency. A repeated
# lookup must retain the same bundled Paimon::c target instead of treating it
# as an external override.
find_package(PaimonCpp CONFIG REQUIRED)
paimon_add_no_runtime_plugin(
  paimon_install_tree_consumer
  SOURCES plugin.cpp)

option(PAIMON_INJECT_FORBIDDEN_RUNTIME "Test the installed ELF guard" OFF)
if(PAIMON_INJECT_FORBIDDEN_RUNTIME)
  target_link_options(
    paimon_install_tree_consumer PRIVATE -Wl,--no-as-needed)
  target_link_libraries(paimon_install_tree_consumer PRIVATE gcc_s)
endif()
