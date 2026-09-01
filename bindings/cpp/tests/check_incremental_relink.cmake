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

foreach(required IN ITEMS BUILD_DIR SOURCE LIBRARY TARGET)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "missing -D${required}=...")
  endif()
endforeach()

file(SHA256 "${LIBRARY}" before_hash)
file(READ "${SOURCE}" source_text)
if(source_text MATCHES "\\+ 1001")
  string(REPLACE "+ 1001" "+ 1002" source_text "${source_text}")
elseif(source_text MATCHES "\\+ 1002")
  string(REPLACE "+ 1002" "+ 1001" source_text "${source_text}")
else()
  message(FATAL_ERROR "relink probe source does not contain its toggle")
endif()
file(WRITE "${SOURCE}" "${source_text}")

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --target "${TARGET}"
  RESULT_VARIABLE build_result
  OUTPUT_VARIABLE build_stdout
  ERROR_VARIABLE build_stderr)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR
          "incremental plugin rebuild failed:\n${build_stdout}\n${build_stderr}")
endif()

file(SHA256 "${LIBRARY}" after_hash)
if(before_hash STREQUAL after_hash)
  message(FATAL_ERROR
          "plugin did not relink after its C++ object archive changed")
endif()
