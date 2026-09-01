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
project(PaimonCpp VERSION 0.1.0 LANGUAGES C CXX)

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
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" paimon_system_processor)
  if(paimon_system_processor MATCHES "^(x86_64|amd64)$")
    set(paimon_linux_rust_target x86_64-unknown-linux-gnu)
  elseif(paimon_system_processor MATCHES "^(aarch64|arm64)$")
    set(paimon_linux_rust_target aarch64-unknown-linux-gnu)
  else()
    message(
      FATAL_ERROR
        "Unsupported Linux architecture for paimon-c: ${CMAKE_SYSTEM_PROCESSOR}")
  endif()
  set(paimon_c_library
      "${paimon_rust_root}/target/${paimon_linux_rust_target}/release/libpaimon_c.so")
  set(paimon_c_build_command
      "${CMAKE_COMMAND}" -E env "MAKEFLAGS="
      "PAIMON_LINUX_RUST_TARGET=${paimon_linux_rust_target}"
      "${CMAKE_CURRENT_SOURCE_DIR}/../c/scripts/build_linux_release.sh")
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

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PaimonNoRuntimePlugin.cmake")

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
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(
      paimon_cpp_header_smoke
      PRIVATE
        -fno-exceptions
        -fno-rtti
        -fvisibility=hidden
        -fvisibility-inlines-hidden)
  endif()
  target_link_libraries(paimon_cpp_header_smoke PRIVATE Paimon::cpp)
  add_test(
    NAME paimon_cpp_header_compile_smoke
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
            --target paimon_cpp_header_smoke)

  if(UNIX AND NOT APPLE)
    # Compile as C++, but deliberately invoke the C linker driver. This proves
    # the facade itself needs no libstdc++/libc++ symbols while resolving every
    # wrapped function against the real libpaimon_c artifact.
    set(paimon_cpp_smoke_object
        "${CMAKE_CURRENT_BINARY_DIR}/paimon_cpp_real_link_smoke.o")
    set(paimon_cpp_smoke_library
        "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}paimon_cpp_real_link_smoke${CMAKE_SHARED_LIBRARY_SUFFIX}")
    get_filename_component(
      paimon_c_library_dir "${paimon_c_library}" DIRECTORY)
    add_custom_command(
      OUTPUT "${paimon_cpp_smoke_object}"
      COMMAND
        "${CMAKE_CXX_COMPILER}" -std=c++17 -fPIC -fno-exceptions -fno-rtti
        -fvisibility=hidden -fvisibility-inlines-hidden
        "-I${CMAKE_CURRENT_SOURCE_DIR}/include"
        "-I${paimon_c_include_dir}"
        -c "${CMAKE_CURRENT_SOURCE_DIR}/tests/header_smoke.cpp"
        -o "${paimon_cpp_smoke_object}"
      DEPENDS
        tests/header_smoke.cpp
        include/paimon/paimon.hpp
        "${paimon_c_include_dir}/paimon.h"
      VERBATIM)
    add_custom_command(
      OUTPUT "${paimon_cpp_smoke_library}"
      COMMAND
        "${CMAKE_C_COMPILER}" -shared
        "${paimon_cpp_smoke_object}"
        "-L${paimon_c_library_dir}" -lpaimon_c -Wl,-z,defs
        -o "${paimon_cpp_smoke_library}"
      DEPENDS "${paimon_cpp_smoke_object}" "${paimon_c_library}"
      VERBATIM)
    add_custom_target(
      paimon_cpp_real_link_smoke ALL DEPENDS "${paimon_cpp_smoke_library}")

    # The helper always compiles with the configured CXX compiler and links the
    # resulting object with the configured C compiler. This remains correct when
    # those drivers are different versions, as on an older deployment host.
    paimon_add_no_runtime_plugin(
      paimon_cpp_no_runtime_plugin
      SOURCES tests/no_cpp_runtime_plugin.cpp
      INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/tests/helper_config"
      COMPILE_DEFINITIONS PAIMON_CPP_HELPER_COMPILE_DEFINITION=73)

    configure_file(
      tests/relink_probe.cpp
      "${CMAKE_CURRENT_BINARY_DIR}/relink_probe.cpp"
      COPYONLY)
    paimon_add_no_runtime_plugin(
      paimon_cpp_relink_probe
      SOURCES "${CMAKE_CURRENT_BINARY_DIR}/relink_probe.cpp")

    add_executable(paimon_cpp_dlopen_smoke tests/dlopen_smoke.c)
    target_link_libraries(paimon_cpp_dlopen_smoke PRIVATE ${CMAKE_DL_LIBS})

    add_test(
      NAME paimon_c_no_cpp_runtime
      COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_linux_elf.sh"
              "${paimon_c_library}")
    add_test(
      NAME paimon_cpp_facade_no_cpp_runtime
      COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_linux_elf.sh"
              "${paimon_cpp_smoke_library}")
    add_test(
      NAME paimon_cpp_plugin_no_cpp_runtime
      COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_linux_elf.sh"
              "$<TARGET_FILE:paimon_cpp_no_runtime_plugin>")

    add_library(
      paimon_elf_fixture_undefined_operator_new SHARED
      tests/elf_fixtures/undefined_operator_new.c)
    add_test(
      NAME paimon_elf_guard_rejects_operator_new
      COMMAND
        "${CMAKE_COMMAND}"
        "-DVERIFIER=${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_linux_elf.sh"
        "-DLIBRARY=$<TARGET_FILE:paimon_elf_fixture_undefined_operator_new>"
        "-DEXPECTED=C++ mangled"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_elf_rejected.cmake")

    add_library(
      paimon_elf_fixture_undefined_host_hook SHARED
      tests/elf_fixtures/undefined_host_hook.c)
    add_test(
      NAME paimon_elf_guard_rejects_host_hook
      COMMAND
        "${CMAKE_COMMAND}"
        "-DVERIFIER=${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_linux_elf.sh"
        "-DLIBRARY=$<TARGET_FILE:paimon_elf_fixture_undefined_host_hook>"
        "-DEXPECTED=unversioned undefined symbol"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_elf_rejected.cmake")

    add_library(
      paimon_elf_fixture_executable_stack SHARED
      tests/elf_fixtures/executable_stack.c)
    target_link_options(
      paimon_elf_fixture_executable_stack PRIVATE -Wl,-z,execstack)
    add_test(
      NAME paimon_elf_guard_rejects_executable_stack
      COMMAND
        "${CMAKE_COMMAND}"
        "-DVERIFIER=${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_linux_elf.sh"
        "-DLIBRARY=$<TARGET_FILE:paimon_elf_fixture_executable_stack>"
        "-DEXPECTED=executable GNU_STACK"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_elf_rejected.cmake")

    add_executable(
      paimon_elf_fixture_pie
      tests/elf_fixtures/pie_executable.c)
    target_compile_options(paimon_elf_fixture_pie PRIVATE -fPIE)
    target_link_options(paimon_elf_fixture_pie PRIVATE -pie)
    add_test(
      NAME paimon_elf_guard_rejects_pie_executable
      COMMAND
        "${CMAKE_COMMAND}"
        "-DVERIFIER=${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_linux_elf.sh"
        "-DLIBRARY=$<TARGET_FILE:paimon_elf_fixture_pie>"
        "-DEXPECTED=PIE executable"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_elf_rejected.cmake")

    add_test(
      NAME paimon_cpp_plugin_incremental_relink
      COMMAND
        "${CMAKE_COMMAND}"
        "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_BINARY_DIR}/relink_probe.cpp"
        "-DLIBRARY=$<TARGET_FILE:paimon_cpp_relink_probe>"
        "-DTARGET=paimon_cpp_relink_probe"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_incremental_relink.cmake")

    add_test(
      NAME paimon_cpp_plugin_isolated_load
      COMMAND
        "${CMAKE_COMMAND}"
        "-DLOADER=$<TARGET_FILE:paimon_cpp_dlopen_smoke>"
        "-DPLUGIN=$<TARGET_FILE:paimon_cpp_no_runtime_plugin>"
        "-DPAIMON_C_LIBRARY_UNDER_TEST=${paimon_c_library}"
        "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/isolated-load-test"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_isolated_load.cmake")

    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
      add_library(
        paimon_elf_fixture_libgcc SHARED
        tests/elf_fixtures/needs_libgcc.c)
      target_link_options(
        paimon_elf_fixture_libgcc PRIVATE -Wl,--no-as-needed)
      target_link_libraries(paimon_elf_fixture_libgcc PRIVATE gcc_s)
      add_test(
        NAME paimon_elf_guard_rejects_libgcc
        COMMAND
          "${CMAKE_COMMAND}"
          "-DVERIFIER=${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_linux_elf.sh"
          "-DLIBRARY=$<TARGET_FILE:paimon_elf_fixture_libgcc>"
          "-DEXPECTED=libgcc_s"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_elf_rejected.cmake")
    endif()

    set(paimon_cpp_install_test_root
        "${CMAKE_CURRENT_BINARY_DIR}/install-tree-consumer-test")
    add_test(
      NAME paimon_cpp_install_tree_consumer
      COMMAND
        "${CMAKE_COMMAND}"
        "-DMAIN_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DCONSUMER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/install_tree_consumer"
        "-DTEST_ROOT=${paimon_cpp_install_test_root}"
        "-DC_COMPILER=${CMAKE_C_COMPILER}"
        "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DPLUGIN_FILENAME=${CMAKE_SHARED_LIBRARY_PREFIX}paimon_install_tree_consumer${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "-DVERIFIER=${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_linux_elf.sh"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_install_tree_consumer.cmake")
    add_test(
      NAME paimon_cpp_install_tree_guard_rejects_runtime
      COMMAND
        "${CMAKE_COMMAND}"
        "-DMAIN_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DCONSUMER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/install_tree_consumer"
        "-DTEST_ROOT=${paimon_cpp_install_test_root}-forbidden"
        "-DC_COMPILER=${CMAKE_C_COMPILER}"
        "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DPLUGIN_FILENAME=${CMAKE_SHARED_LIBRARY_PREFIX}paimon_install_tree_consumer${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "-DVERIFIER=${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_linux_elf.sh"
        -DEXPECT_BUILD_FAILURE=ON
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_install_tree_consumer.cmake")
  endif()
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
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PaimonNoRuntimePlugin.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/PaimonCpp"
  COMPONENT ${paimon_cpp_install_component})
install(
  PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_linux_elf.sh"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/PaimonCpp"
  COMPONENT ${paimon_cpp_install_component})

# Keep the normal CMake install target, and also materialize a ready-to-use SDK
# beside the build files so `cmake --build` has an obvious deliverable.
set(paimon_cpp_sdk_dir "${CMAKE_CURRENT_BINARY_DIR}/sdk")
add_custom_target(
  paimon_cpp_sdk ALL
  COMMAND
    "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
    --prefix "${paimon_cpp_sdk_dir}"
    --component ${paimon_cpp_install_component}
  DEPENDS paimon_c_cargo_build
  COMMENT "Staging the Paimon C++ SDK in ${paimon_cpp_sdk_dir}"
  VERBATIM
  USES_TERMINAL)
