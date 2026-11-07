cmake_minimum_required(VERSION 3.16)
project(dmotor_software LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(DMOTOR_SW_SANITIZE "Enable native undefined-behavior checks" OFF)
add_library(dmotor_semantics src/model.cpp src/runtime.cpp src/protocol.cpp src/monitor_protocol.cpp src/version.cpp ../src/atomic_mailbox.cpp)
target_include_directories(dmotor_semantics PUBLIC include ../include)
target_compile_options(dmotor_semantics PRIVATE -Wall -Wextra -Werror -Wconversion)
add_executable(dmotor-semantic-tests tests/model_test.cpp)
target_link_libraries(dmotor-semantic-tests PRIVATE dmotor_semantics)
target_compile_options(dmotor-semantic-tests PRIVATE -Wall -Wextra -Werror -Wconversion)
if(DMOTOR_SW_SANITIZE)
  target_compile_options(dmotor_semantics PUBLIC -fsanitize=undefined -fno-sanitize-recover=all)
  target_link_options(dmotor_semantics PUBLIC -fsanitize=undefined)
endif()
enable_testing()
add_test(NAME software_semantics COMMAND dmotor-semantic-tests)
install(TARGETS dmotor-semantic-tests RUNTIME DESTINATION bin)

add_executable(dmotor-protocol-tests tests/protocol_test.cpp)
target_link_libraries(dmotor-protocol-tests PRIVATE dmotor_semantics)
target_compile_options(dmotor-protocol-tests PRIVATE -Wall -Wextra -Werror -Wconversion)
add_test(NAME software_protocol COMMAND dmotor-protocol-tests)
install(TARGETS dmotor-protocol-tests RUNTIME DESTINATION bin)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(dmotor-uart-test apps/uart_test.cpp)
  target_link_libraries(dmotor-uart-test PRIVATE dmotor_semantics)
  target_compile_options(dmotor-uart-test PRIVATE -Wall -Wextra -Werror -Wconversion)
  install(TARGETS dmotor-uart-test RUNTIME DESTINATION bin)
  include(GNUInstallDirs)
  include(CMakePackageConfigHelpers)
  add_library(dmotor_client STATIC linux/client.cpp)
  add_library(dmotor::client ALIAS dmotor_client)
  set_target_properties(dmotor_client PROPERTIES EXPORT_NAME client POSITION_INDEPENDENT_CODE ON)
  target_compile_features(dmotor_client PUBLIC cxx_std_14)
  target_include_directories(dmotor_client PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  target_compile_options(dmotor_client PRIVATE -Wall -Wextra -Werror -Wconversion)
  if(DMOTOR_SW_SANITIZE)
    target_compile_options(dmotor_client PUBLIC -fsanitize=undefined -fno-sanitize-recover=all)
    target_link_options(dmotor_client PUBLIC -fsanitize=undefined)
  endif()
  install(TARGETS dmotor_client EXPORT dmotorClientTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(FILES include/dmotor_sw/client.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dmotor_sw)
  install(EXPORT dmotorClientTargets NAMESPACE dmotor::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dmotorClient)
  configure_package_config_file(cmake/dmotorClientConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/dmotorClientConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dmotorClient)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dmotorClientConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dmotorClient)
  add_library(dmotor_linux linux/inspection.cpp linux/kernel_device.cpp linux/monitor_uart.cpp)
  target_link_libraries(dmotor_linux PUBLIC dmotor_semantics)
  target_compile_options(dmotor_linux PRIVATE -Wall -Wextra -Werror -Wconversion)
  foreach(app daemon maintenance)
    if(app STREQUAL "daemon")
      set(executable dmotord)
    else()
      set(executable dmotor-maint)
    endif()
    add_executable(${executable} apps/${app}.cpp)
    if(app STREQUAL "maintenance")
      target_link_libraries(${executable} PRIVATE dmotor_client dmotor_semantics)
    else()
      target_link_libraries(${executable} PRIVATE dmotor_linux)
    endif()
    target_compile_options(${executable} PRIVATE -Wall -Wextra -Werror -Wconversion)
    install(TARGETS ${executable} RUNTIME DESTINATION bin)
  endforeach()
  add_executable(dmotor-encoder apps/encoder.cpp)
  target_link_libraries(dmotor-encoder PRIVATE dmotor_linux)
  target_compile_options(dmotor-encoder PRIVATE -Wall -Wextra -Werror -Wconversion)
  add_executable(dmotor-inspect apps/inspect.cpp)
  target_link_libraries(dmotor-inspect PRIVATE dmotor_linux)
  target_compile_options(dmotor-inspect PRIVATE -Wall -Wextra -Werror -Wconversion)
  add_executable(dmotor-linux-tests tests/linux_inspection_test.cpp)
  target_link_libraries(dmotor-linux-tests PRIVATE dmotor_linux)
  target_compile_options(dmotor-linux-tests PRIVATE -Wall -Wextra -Werror -Wconversion)
  add_test(NAME software_linux COMMAND dmotor-linux-tests)
  install(TARGETS dmotor-encoder dmotor-inspect dmotor-linux-tests RUNTIME DESTINATION bin)
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_target(dmotor_version ALL
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/version.py ${CMAKE_CURRENT_BINARY_DIR}/generated
  BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/generated/dmotor_version_generated.hpp
             ${CMAKE_CURRENT_BINARY_DIR}/generated/source-version.json)
add_dependencies(dmotor_semantics dmotor_version)
target_include_directories(dmotor_semantics PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/source-version.json DESTINATION share/dmotor)

add_executable(dmotor-runtime-tests tests/runtime_test.cpp)
target_link_libraries(dmotor-runtime-tests PRIVATE dmotor_semantics)
target_compile_options(dmotor-runtime-tests PRIVATE -Wall -Wextra -Werror -Wconversion)
add_test(NAME software_runtime COMMAND dmotor-runtime-tests)
install(TARGETS dmotor-runtime-tests RUNTIME DESTINATION bin)

# No deployed mock mode: these executables exist only for native process tests.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CROSSCOMPILING)
  add_executable(dmotord-fixture apps/daemon.cpp tests/fake_kernel.cpp)
  target_compile_definitions(dmotord-fixture PRIVATE DMOTOR_TEST_SERVICE)
  target_link_libraries(dmotord-fixture PRIVATE dmotor_semantics)
  add_executable(dmotor-maint-fixture apps/maintenance.cpp)
  target_compile_definitions(dmotor-maint-fixture PRIVATE DMOTOR_TEST_SERVICE)
  target_link_libraries(dmotor-maint-fixture PRIVATE dmotor_client dmotor_semantics)
  add_executable(dmotor-client-tests tests/client_test.cpp)
  target_link_libraries(dmotor-client-tests PRIVATE dmotor_client)
  target_compile_options(dmotor-client-tests PRIVATE -Wall -Wextra -Werror -Wconversion)
  add_test(NAME software_client COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/client_test.py $<TARGET_FILE:dmotor-client-tests>)
  add_test(NAME software_client_package COMMAND ${CMAKE_COMMAND}
    -DPROJECT_BUILD=${CMAKE_CURRENT_BINARY_DIR}
    -DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/client_consumer
    -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/client-package-test
    -DTEST_COMPILER=${CMAKE_CXX_COMPILER}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/client_package_test.cmake)
  add_test(NAME software_startup COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/startup_test.py $<TARGET_FILE:dmotord-fixture> $<TARGET_FILE:dmotor-maint-fixture>)
  add_test(NAME software_powered COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/service_test.py $<TARGET_FILE:dmotord-fixture> $<TARGET_FILE:dmotor-maint-fixture> --powered)
  add_test(NAME software_direct COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/service_test.py $<TARGET_FILE:dmotord-fixture> $<TARGET_FILE:dmotor-maint-fixture> --direct)
  add_test(NAME software_commissioning COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/service_test.py $<TARGET_FILE:dmotord-fixture> $<TARGET_FILE:dmotor-maint-fixture> --commissioning)
  add_test(NAME software_service COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/service_test.py $<TARGET_FILE:dmotord-fixture> $<TARGET_FILE:dmotor-maint-fixture>)
endif()

install(FILES config/powered-direct-duty.conf config/output-validation.conf config/inhibited-example.conf DESTINATION share/dmotor)

add_executable(dmotor-monitor-tests tests/monitor_protocol_test.cpp)
target_link_libraries(dmotor-monitor-tests PRIVATE dmotor_semantics)
target_compile_options(dmotor-monitor-tests PRIVATE -Wall -Wextra -Werror -Wconversion)
add_test(NAME software_monitor COMMAND dmotor-monitor-tests)
install(TARGETS dmotor-monitor-tests RUNTIME DESTINATION bin)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(dmotor-monitor-uart-tests tests/monitor_uart_test.cpp)
  target_link_libraries(dmotor-monitor-uart-tests PRIVATE dmotor_linux)
  target_compile_options(dmotor-monitor-uart-tests PRIVATE -Wall -Wextra -Werror -Wconversion)
  add_test(NAME software_monitor_uart COMMAND dmotor-monitor-uart-tests)
  install(TARGETS dmotor-monitor-uart-tests RUNTIME DESTINATION bin)
endif()

install(FILES protocol/monitor/wire.json protocol/monitor/README.md DESTINATION share/dmotor/monitor)

add_test(NAME software_monitor_schema COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/protocol/monitor/generate.py --check)

# Post-pairing recovery model: deliberately not linked into daemon/runtime or installed.
add_executable(dmotor-recovery-model-tests tests/recovery_model_test.cpp src/recovery_model.cpp)
target_include_directories(dmotor-recovery-model-tests PRIVATE include)
target_compile_options(dmotor-recovery-model-tests PRIVATE -Wall -Wextra -Werror -Wconversion)
if(DMOTOR_SW_SANITIZE)
  target_compile_options(dmotor-recovery-model-tests PRIVATE -fsanitize=undefined -fno-sanitize-recover=all)
  target_link_options(dmotor-recovery-model-tests PRIVATE -fsanitize=undefined)
endif()
add_test(NAME software_recovery_model COMMAND dmotor-recovery-model-tests)

# Isolated Linux session-store tests; not linked into the service or installed.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(dmotor-endpoint-session-tests tests/endpoint_session_test.cpp linux/endpoint_session.cpp)
  target_include_directories(dmotor-endpoint-session-tests PRIVATE include)
  target_compile_options(dmotor-endpoint-session-tests PRIVATE -Wall -Wextra -Werror -Wconversion)
  target_link_options(dmotor-endpoint-session-tests PRIVATE -Wl,--wrap=write -Wl,--wrap=fsync)
  if(DMOTOR_SW_SANITIZE)
    target_compile_options(dmotor-endpoint-session-tests PRIVATE -fsanitize=undefined -fno-sanitize-recover=all)
    target_link_options(dmotor-endpoint-session-tests PRIVATE -fsanitize=undefined)
  endif()
  add_test(NAME software_endpoint_session COMMAND dmotor-endpoint-session-tests)
endif()

# Symmetric pairing/replay model only; no service installation or live admission.
add_executable(dmotor-session-link-tests tests/session_link_test.cpp src/session_link.cpp)
target_link_libraries(dmotor-session-link-tests PRIVATE dmotor_semantics)
target_compile_options(dmotor-session-link-tests PRIVATE -Wall -Wextra -Werror -Wconversion)
add_test(NAME software_session_link COMMAND dmotor-session-link-tests)

# Isolated PS adapter with scripted monitor fixture; never installed.
add_executable(dmotor-recovery-link-tests tests/recovery_link_test.cpp src/recovery_link.cpp src/recovery_model.cpp src/session_link.cpp)
target_link_libraries(dmotor-recovery-link-tests PRIVATE dmotor_semantics)
target_compile_options(dmotor-recovery-link-tests PRIVATE -Wall -Wextra -Werror -Wconversion)
add_test(NAME software_recovery_link COMMAND dmotor-recovery-link-tests)

# Host-only behavioral monitor and joint recovery simulation; never installed.
add_executable(dmotor-monitor-simulator-tests tests/monitor_simulator_test.cpp tests/support/monitor_simulator.cpp src/recovery_link.cpp src/recovery_model.cpp src/session_link.cpp)
target_link_libraries(dmotor-monitor-simulator-tests PRIVATE dmotor_semantics)
target_compile_options(dmotor-monitor-simulator-tests PRIVATE -Wall -Wextra -Werror -Wconversion)
add_test(NAME software_monitor_simulator COMMAND dmotor-monitor-simulator-tests)

# Host coordinator integration only; no daemon registration or installation.
add_executable(dmotor-recovery-coordinator-tests tests/recovery_coordinator_test.cpp tests/support/monitor_simulator.cpp src/recovery_coordinator.cpp src/recovery_link.cpp src/recovery_model.cpp src/session_link.cpp)
target_link_libraries(dmotor-recovery-coordinator-tests PRIVATE dmotor_semantics)
target_compile_options(dmotor-recovery-coordinator-tests PRIVATE -Wall -Wextra -Werror -Wconversion)
add_test(NAME software_recovery_coordinator COMMAND dmotor-recovery-coordinator-tests)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(dmotor-coordinator-session-tests tests/coordinator_session_test.cpp linux/endpoint_session.cpp src/recovery_coordinator.cpp src/recovery_link.cpp src/recovery_model.cpp src/session_link.cpp)
  target_link_libraries(dmotor-coordinator-session-tests PRIVATE dmotor_semantics)
  target_compile_options(dmotor-coordinator-session-tests PRIVATE -Wall -Wextra -Werror -Wconversion)
  target_link_options(dmotor-coordinator-session-tests PRIVATE -Wl,--wrap=fsync)
  add_test(NAME software_coordinator_session COMMAND dmotor-coordinator-session-tests)
endif()

add_executable(dmotor-uart-codec-tests tests/uart_diagnostic_codec_test.cpp)
target_link_libraries(dmotor-uart-codec-tests PRIVATE dmotor_semantics)
target_compile_options(dmotor-uart-codec-tests PRIVATE -Wall -Wextra -Werror -Wconversion)
add_test(NAME software_uart_diagnostic_codec COMMAND dmotor-uart-codec-tests)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_test(NAME software_uart_diagnostic_pty COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/uart_diagnostic_test.py $<TARGET_FILE:dmotor-uart-test>)
endif()

# Developer tools are opt-in and are not dependencies of an EDF target build.
option(DMOTOR_SW_CLANG_TIDY "Run clang-tidy 18 while compiling PS software" OFF)
if(DMOTOR_SW_CLANG_TIDY)
  find_program(DMOTOR_CLANG_TIDY NAMES clang-tidy-18)
  if(NOT DMOTOR_CLANG_TIDY)
    message(FATAL_ERROR "DMOTOR_SW_CLANG_TIDY requires clang-tidy-18")
  endif()
  get_property(dmotor_sw_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
  foreach(target IN LISTS dmotor_sw_targets)
    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL "EXECUTABLE" OR target_type STREQUAL "STATIC_LIBRARY")
      set_property(TARGET ${target} PROPERTY CXX_CLANG_TIDY "${DMOTOR_CLANG_TIDY}")
    endif()
  endforeach()
endif()
add_custom_target(ps-format
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/format.py
  COMMENT "Format handwritten PS userspace C++ (clang-format 18)")
add_custom_target(ps-format-check
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/format.py --check
  COMMENT "Check handwritten PS userspace C++ formatting")

# Pure U1 diagnostic policy; no IDF dependency or hardware access.
add_executable(dmotor-gpio-diagnostic-tests tests/gpio_diagnostic_test.cpp)
target_link_libraries(dmotor-gpio-diagnostic-tests PRIVATE dmotor_semantics)
target_compile_options(dmotor-gpio-diagnostic-tests PRIVATE -Wall -Wextra -Werror -Wconversion)
add_test(NAME software_gpio_diagnostic COMMAND dmotor-gpio-diagnostic-tests)
