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

foreach(required IN ITEMS MAIN_BUILD_DIR CONSUMER_SOURCE_DIR TEST_ROOT
                          C_COMPILER CXX_COMPILER PLUGIN_FILENAME VERIFIER)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "missing -D${required}=...")
  endif()
endforeach()

set(test_prefix "${TEST_ROOT}/prefix")
set(consumer_build "${TEST_ROOT}/build")
if(NOT DEFINED EXPECT_BUILD_FAILURE)
  set(EXPECT_BUILD_FAILURE OFF)
endif()
file(REMOVE_RECURSE "${TEST_ROOT}")

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${MAIN_BUILD_DIR}"
          --prefix "${test_prefix}"
  RESULT_VARIABLE install_result
  OUTPUT_VARIABLE install_stdout
  ERROR_VARIABLE install_stderr)
if(NOT install_result EQUAL 0)
  message(FATAL_ERROR
          "install-tree setup failed:\n${install_stdout}\n${install_stderr}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
          -S "${CONSUMER_SOURCE_DIR}"
          -B "${consumer_build}"
          "-DCMAKE_PREFIX_PATH=${test_prefix}"
          "-DCMAKE_C_COMPILER=${C_COMPILER}"
          "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
          "-DPAIMON_INJECT_FORBIDDEN_RUNTIME=${EXPECT_BUILD_FAILURE}"
  RESULT_VARIABLE configure_result
  OUTPUT_VARIABLE configure_stdout
  ERROR_VARIABLE configure_stderr)
if(NOT configure_result EQUAL 0)
  message(FATAL_ERROR
          "install-tree consumer configure failed:\n${configure_stdout}\n${configure_stderr}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${consumer_build}"
  RESULT_VARIABLE build_result
  OUTPUT_VARIABLE build_stdout
  ERROR_VARIABLE build_stderr)
if(EXPECT_BUILD_FAILURE)
  if(build_result EQUAL 0)
    message(FATAL_ERROR
            "installed helper accepted a forbidden runtime dependency")
  endif()
  set(build_output "${build_stdout}\n${build_stderr}")
  if(NOT build_output MATCHES "forbidden non-C runtime dependency")
    message(FATAL_ERROR
            "consumer failed for the wrong reason:\n${build_output}")
  endif()
  return()
endif()
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR
          "install-tree consumer build failed:\n${build_stdout}\n${build_stderr}")
endif()

set(plugin "${consumer_build}/${PLUGIN_FILENAME}")
execute_process(
  COMMAND "${VERIFIER}" "${plugin}"
  RESULT_VARIABLE verifier_result
  OUTPUT_VARIABLE verifier_stdout
  ERROR_VARIABLE verifier_stderr)
if(NOT verifier_result EQUAL 0)
  message(FATAL_ERROR
          "installed no-runtime plugin failed ELF verification:\n${verifier_stdout}\n${verifier_stderr}")
endif()
