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

foreach(required IN ITEMS VERIFIER LIBRARY EXPECTED)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "missing -D${required}=...")
  endif()
endforeach()

execute_process(
  COMMAND "${VERIFIER}" "${LIBRARY}"
  RESULT_VARIABLE verifier_result
  OUTPUT_VARIABLE verifier_stdout
  ERROR_VARIABLE verifier_stderr)
set(verifier_output "${verifier_stdout}\n${verifier_stderr}")

if(verifier_result EQUAL 0)
  message(FATAL_ERROR
          "ELF verifier accepted forbidden fixture ${LIBRARY}:\n${verifier_output}")
endif()
string(FIND "${verifier_output}" "${EXPECTED}" expected_index)
if(expected_index EQUAL -1)
  message(FATAL_ERROR
          "ELF verifier did not report '${EXPECTED}':\n${verifier_output}")
endif()
