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
project(PaimonCpp VERSION 0.1.0 LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(PAIMON_CPP_BUILD_EXAMPLES "Build the C++ facade examples" OFF)
option(PAIMON_CPP_BUILD_TESTS "Build the header compile smoke test" OFF)
get_filename_component(
  paimon_rust_root "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(paimon_c_include_dir "${CMAKE_CURRENT_SOURCE_DIR}/../c/include")

if(TARGET Paimon::c)
  message(
    FATAL_ERROR
      "bindings/cpp owns Paimon::c and does not accept an external C ABI target")
endif()

if(DEFINED ENV{CARGO} AND EXISTS "$ENV{CARGO}")
  set(paimon_cargo_executable "$ENV{CARGO}")
elseif(DEFINED ENV{HOME} AND EXISTS "$ENV{HOME}/.cargo/bin/cargo")
  set(paimon_cargo_executable "$ENV{HOME}/.cargo/bin/cargo")
else()
  set(paimon_find_appbundle "${CMAKE_FIND_APPBUNDLE}")
  set(CMAKE_FIND_APPBUNDLE NEVER)
  find_program(paimon_cargo_executable NAMES cargo)
  set(CMAKE_FIND_APPBUNDLE "${paimon_find_appbundle}")
endif()
if(NOT paimon_cargo_executable)
  message(FATAL_ERROR "Rust Cargo was not found")
endif()
execute_process(
  COMMAND "${paimon_cargo_executable}" --version
  RESULT_VARIABLE paimon_cargo_version_result
  OUTPUT_VARIABLE paimon_cargo_version
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT paimon_cargo_version_result EQUAL 0 OR
   NOT paimon_cargo_version MATCHES "^cargo [0-9]")
  message(FATAL_ERROR "Not a Rust Cargo executable: ${paimon_cargo_executable}")
endif()
if(APPLE)
  set(paimon_c_library "${paimon_rust_root}/target/release/libpaimon_c.dylib")
  set(paimon_c_build_command
      "${CMAKE_COMMAND}" -E env "MAKEFLAGS=" "${paimon_cargo_executable}"
      build --locked --release -p paimon-c)
elseif(UNIX)
  set(paimon_c_library "${paimon_rust_root}/target/release/libpaimon_c.so")
  set(paimon_c_build_command
      "${CMAKE_COMMAND}" -E env "MAKEFLAGS=" "${paimon_cargo_executable}"
      build --locked --release -p paimon-c)
elseif(WIN32)
  set(paimon_c_library "${paimon_rust_root}/target/release/paimon_c.dll")
  set(paimon_c_build_command
      "${CMAKE_COMMAND}" -E env "MAKEFLAGS=" "${paimon_cargo_executable}"
      build --locked --release -p paimon-c)
else()
  message(FATAL_ERROR "Unsupported platform for automatic paimon-c build")
endif()

add_custom_target(
  paimon_c_cargo_build ALL
  COMMAND ${paimon_c_build_command}
  WORKING_DIRECTORY "${paimon_rust_root}"
  BYPRODUCTS "${paimon_c_library}"
  COMMENT "Building the in-tree Rust paimon-c library"
  VERBATIM
  USES_TERMINAL)

get_filename_component(
  PAIMON_C_INSTALL_FILENAME "${paimon_c_library}" NAME)

# Keep the C ABI dependency as a target in both the build and install trees.
# PaimonCppConfig.cmake recreates this imported target for installed consumers.
add_library(Paimon::c SHARED IMPORTED GLOBAL)
set_target_properties(
  Paimon::c
  PROPERTIES
    IMPORTED_LOCATION "${paimon_c_library}"
    IMPORTED_NO_SONAME TRUE
    INTERFACE_INCLUDE_DIRECTORIES "${paimon_c_include_dir}")
add_dependencies(Paimon::c paimon_c_cargo_build)

add_library(paimon_cpp INTERFACE)
add_library(Paimon::cpp ALIAS paimon_cpp)
add_dependencies(paimon_cpp paimon_c_cargo_build)
set_target_properties(paimon_cpp PROPERTIES EXPORT_NAME cpp)
target_compile_features(paimon_cpp INTERFACE cxx_std_17)
target_include_directories(
  paimon_cpp
  INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

target_include_directories(
  paimon_cpp INTERFACE "$<BUILD_INTERFACE:${paimon_c_include_dir}>")

target_link_libraries(paimon_cpp INTERFACE Paimon::c)

if(PAIMON_CPP_BUILD_EXAMPLES)
  add_executable(paimon_cpp_batch_read examples/batch_read.cpp)
  target_link_libraries(paimon_cpp_batch_read PRIVATE Paimon::cpp)
  add_executable(paimon_cpp_streaming_write examples/streaming_write.cpp)
  target_link_libraries(paimon_cpp_streaming_write PRIVATE Paimon::cpp)
  add_executable(paimon_cpp_stream_read examples/stream_read.cpp)
  target_link_libraries(paimon_cpp_stream_read PRIVATE Paimon::cpp)
endif()

if(PAIMON_CPP_BUILD_TESTS)
  enable_testing()
  add_library(paimon_cpp_header_smoke OBJECT tests/header_smoke.cpp)
  target_compile_definitions(
    paimon_cpp_header_smoke
    PRIVATE PAIMON_C_HEADER="paimon_test_stub.h")
  target_include_directories(
    paimon_cpp_header_smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  target_link_libraries(paimon_cpp_header_smoke PRIVATE Paimon::cpp)
  add_test(
    NAME paimon_cpp_header_compile_smoke
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
            --target paimon_cpp_header_smoke)

  set(paimon_cpp_install_test_root
      "${CMAKE_CURRENT_BINARY_DIR}/install-tree-consumer-test")
  add_test(
    NAME paimon_cpp_install_tree_consumer
    COMMAND
      "${CMAKE_COMMAND}"
      "-DMAIN_BUILD_DIR=${CMAKE_BINARY_DIR}"
      "-DCONSUMER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/install_tree_consumer"
      "-DTEST_ROOT=${paimon_cpp_install_test_root}"
      "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_install_tree_consumer.cmake")
endif()

set(paimon_cpp_install_component PaimonCppSdk)
install(
  DIRECTORY include/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT ${paimon_cpp_install_component})
install(
  FILES "${paimon_c_include_dir}/paimon.h"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT ${paimon_cpp_install_component})
install(
  FILES "${paimon_c_library}"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  COMPONENT ${paimon_cpp_install_component})
if(APPLE)
  find_program(paimon_install_name_tool NAMES install_name_tool)
  find_program(paimon_codesign NAMES codesign)
  if(NOT paimon_install_name_tool OR NOT paimon_codesign)
    message(FATAL_ERROR "install_name_tool and codesign are required on macOS")
  endif()
  install(
    CODE
      "set(paimon_installed_library \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/${PAIMON_C_INSTALL_FILENAME}\")
       execute_process(
         COMMAND \"${paimon_install_name_tool}\" -id \"@rpath/${PAIMON_C_INSTALL_FILENAME}\" \"\${paimon_installed_library}\"
         RESULT_VARIABLE paimon_install_name_result)
       if(NOT paimon_install_name_result EQUAL 0)
         message(FATAL_ERROR \"failed to set the paimon-c install name\")
       endif()
       execute_process(
         COMMAND \"${paimon_codesign}\" --force --sign - \"\${paimon_installed_library}\"
         RESULT_VARIABLE paimon_codesign_result)
       if(NOT paimon_codesign_result EQUAL 0)
         message(FATAL_ERROR \"failed to sign the installed paimon-c library\")
       endif()"
    COMPONENT ${paimon_cpp_install_component})
endif()
install(
  TARGETS paimon_cpp
  EXPORT PaimonCppTargets
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(
  EXPORT PaimonCppTargets
  FILE PaimonCppTargets.cmake
  NAMESPACE Paimon::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/PaimonCpp"
  COMPONENT ${paimon_cpp_install_component})

configure_package_config_file(
  cmake/PaimonCppConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/PaimonCppConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/PaimonCpp")
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/PaimonCppConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/PaimonCppConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/PaimonCppConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/PaimonCpp"
  COMPONENT ${paimon_cpp_install_component})

# Keep the normal CMake install target, and also materialize the ready-to-use
# headers, library, and package files directly in the CMake build directory.
add_custom_target(
  paimon_cpp_artifacts ALL
  COMMAND
    "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
    --prefix "${CMAKE_CURRENT_BINARY_DIR}"
    --component ${paimon_cpp_install_component}
  DEPENDS paimon_c_cargo_build
  COMMENT "Staging the Paimon C++ artifacts in ${CMAKE_CURRENT_BINARY_DIR}"
  VERBATIM
  USES_TERMINAL)
