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

foreach(required IN ITEMS LOADER PLUGIN PAIMON_C_LIBRARY TEST_ROOT)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "missing -D${required}=...")
  endif()
endforeach()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}")
file(COPY "${PLUGIN}" "${PAIMON_C_LIBRARY}" DESTINATION "${TEST_ROOT}")
get_filename_component(plugin_name "${PLUGIN}" NAME)
execute_process(
  COMMAND "${LOADER}" "./${plugin_name}"
  WORKING_DIRECTORY "${TEST_ROOT}"
  RESULT_VARIABLE load_result
  OUTPUT_VARIABLE load_stdout
  ERROR_VARIABLE load_stderr)
if(NOT load_result EQUAL 0)
  message(FATAL_ERROR
          "isolated plugin load failed:\n${load_stdout}\n${load_stderr}")
endif()
