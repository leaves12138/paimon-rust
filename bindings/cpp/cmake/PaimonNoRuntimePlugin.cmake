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

include_guard(GLOBAL)

# Capture this while the module is included. CMAKE_CURRENT_LIST_DIR inside a
# function can otherwise refer to the consumer's calling list file.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/verify_linux_elf.sh")
  set(_PAIMON_NO_RUNTIME_ELF_VERIFIER
      "${CMAKE_CURRENT_LIST_DIR}/verify_linux_elf.sh")
else()
  set(_PAIMON_NO_RUNTIME_ELF_VERIFIER
      "${CMAKE_CURRENT_LIST_DIR}/../scripts/verify_linux_elf.sh")
endif()

# Build a C-linkage plugin from C++17 sources without linking a C++ runtime.
# The source must itself stay within the no-exceptions/no-RTTI subset used by
# the Paimon facade. The final target is linked by the configured C driver.
function(paimon_add_no_runtime_plugin target)
  if(NOT TARGET Paimon::cpp OR NOT TARGET Paimon::c)
    message(FATAL_ERROR
            "paimon_add_no_runtime_plugin requires Paimon::cpp and Paimon::c")
  endif()
  set(multi_value_args
      SOURCES INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
      LINK_LIBRARIES)
  cmake_parse_arguments(PAIMON_PLUGIN "" "" "${multi_value_args}" ${ARGN})
  if(PAIMON_PLUGIN_KEYWORDS_MISSING_VALUES)
    message(FATAL_ERROR
            "missing values for: ${PAIMON_PLUGIN_KEYWORDS_MISSING_VALUES}")
  endif()
  if(PAIMON_PLUGIN_SOURCES)
    if(PAIMON_PLUGIN_UNPARSED_ARGUMENTS)
      message(FATAL_ERROR
              "unexpected no-runtime plugin arguments: ${PAIMON_PLUGIN_UNPARSED_ARGUMENTS}")
    endif()
    set(plugin_sources ${PAIMON_PLUGIN_SOURCES})
  else()
    # Preserve the original positional-source form.
    set(plugin_sources ${PAIMON_PLUGIN_UNPARSED_ARGUMENTS})
  endif()
  if(NOT plugin_sources)
    message(FATAL_ERROR
            "paimon_add_no_runtime_plugin(${target}) requires source files")
  endif()
  if(TARGET "${target}" OR TARGET "${target}__paimon_cpp_objects")
    message(FATAL_ERROR "target already exists: ${target}")
  endif()
  if(NOT UNIX OR APPLE OR
     NOT CMAKE_C_COMPILER_ID MATCHES "Clang|GNU" OR
     NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR
            "no-runtime plugins currently require GNU/Clang C and C++ compilers on Linux")
  endif()

  if(NOT EXISTS "${_PAIMON_NO_RUNTIME_ELF_VERIFIER}")
    message(FATAL_ERROR
            "Paimon ELF verifier is missing: ${_PAIMON_NO_RUNTIME_ELF_VERIFIER}")
  endif()

  add_library("${target}__paimon_cpp_objects" OBJECT ${plugin_sources})
  set_target_properties(
    "${target}__paimon_cpp_objects"
    PROPERTIES
      POSITION_INDEPENDENT_CODE ON
      CXX_STANDARD 17
      CXX_STANDARD_REQUIRED ON
      CXX_EXTENSIONS OFF)
  target_compile_options(
    "${target}__paimon_cpp_objects"
    PRIVATE
      -fno-exceptions
      -fno-rtti
      -fvisibility=hidden
      -fvisibility-inlines-hidden
      ${PAIMON_PLUGIN_COMPILE_OPTIONS})
  if(PAIMON_PLUGIN_INCLUDE_DIRECTORIES)
    target_include_directories(
      "${target}__paimon_cpp_objects"
      PRIVATE ${PAIMON_PLUGIN_INCLUDE_DIRECTORIES})
  endif()
  if(PAIMON_PLUGIN_COMPILE_DEFINITIONS)
    target_compile_definitions(
      "${target}__paimon_cpp_objects"
      PRIVATE ${PAIMON_PLUGIN_COMPILE_DEFINITIONS})
  endif()
  target_link_libraries(
    "${target}__paimon_cpp_objects"
    PRIVATE Paimon::cpp ${PAIMON_PLUGIN_LINK_LIBRARIES})

  # Hide the C++ object language from the final C target. CMake otherwise adds
  # its configured implicit C++ libraries even when LINKER_LANGUAGE is C.
  set(archive_target "${target}__paimon_cpp_archive")
  add_library(
    "${archive_target}" STATIC
    $<TARGET_OBJECTS:${target}__paimon_cpp_objects>)
  set_target_properties("${archive_target}" PROPERTIES LINKER_LANGUAGE CXX)

  set(link_stub "${CMAKE_CURRENT_BINARY_DIR}/${target}__paimon_link_stub.c")
  file(GENERATE OUTPUT "${link_stub}"
       CONTENT "/* Generated C link anchor for a Paimon no-runtime plugin. */\n")
  add_library("${target}" SHARED "${link_stub}")
  set_target_properties(
    "${target}"
    PROPERTIES
      LINKER_LANGUAGE C
      BUILD_WITH_INSTALL_RPATH TRUE
      INSTALL_RPATH "\$ORIGIN")
  add_dependencies("${target}" "${archive_target}")
  set_property(
    TARGET "${target}" APPEND PROPERTY
    LINK_DEPENDS "$<TARGET_FILE:${archive_target}>")
  target_link_options(
    "${target}"
    PRIVATE
      "-Wl,--whole-archive,$<TARGET_FILE:${archive_target}>,--no-whole-archive"
      -Wl,-z,defs)
  target_link_libraries(
    "${target}" PRIVATE Paimon::c ${PAIMON_PLUGIN_LINK_LIBRARIES})
  add_custom_command(
    TARGET "${target}"
    POST_BUILD
    COMMAND "${_PAIMON_NO_RUNTIME_ELF_VERIFIER}" "$<TARGET_FILE:${target}>"
    COMMENT "Verifying that ${target} has a C-only dynamic ABI"
    VERBATIM)
endfunction()
