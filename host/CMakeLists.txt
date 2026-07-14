# SPDX-FileCopyrightText: 2026 Benoit Quiniou
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.16)
project(vbankey_host C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(COMP ${ROOT}/components)

add_library(vban INTERFACE)
target_include_directories(vban INTERFACE ${COMP}/vban/include)

add_library(common INTERFACE)
target_include_directories(common INTERFACE ${COMP}/common/include)

add_library(tomlc17 STATIC ${COMP}/tomlc17/tomlc17.c)
target_include_directories(tomlc17 PUBLIC ${COMP}/tomlc17)

add_library(unity STATIC ${ROOT}/third_party/unity/unity.c)
target_include_directories(unity PUBLIC ${ROOT}/third_party/unity ${CMAKE_CURRENT_SOURCE_DIR}/unity_shim)
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_CONFIG_H)

# Turns the ESP-IDF-idiomatic TEST_CASE()s into a standalone Unity binary by
# supplying main() + the registration hook; link only into TEST_CASE suites.
add_library(unity_runner STATIC ${CMAKE_CURRENT_SOURCE_DIR}/unity_shim/unity_runner.c)
target_link_libraries(unity_runner PUBLIC unity)

add_library(config STATIC ${COMP}/config/src/config.c)
target_include_directories(config PUBLIC ${COMP}/config/include)
target_link_libraries(config PUBLIC tomlc17 common)

add_library(button STATIC ${COMP}/button/src/button.c)
target_include_directories(button PUBLIC ${COMP}/button/include)
target_link_libraries(button PUBLIC common)

add_library(vban_script STATIC ${COMP}/vban_script/src/vban_script.c)
target_include_directories(vban_script PUBLIC ${COMP}/vban_script/include)
target_link_libraries(vban_script PUBLIC config vban)

add_library(runtime STATIC ${COMP}/runtime/src/runtime.c)
target_include_directories(runtime PUBLIC ${COMP}/runtime/include)
target_link_libraries(runtime PUBLIC config vban_script button)

add_library(vban_net STATIC ${COMP}/vban_net/src/vban_net.c)
target_include_directories(vban_net PUBLIC ${COMP}/vban_net/include)
target_link_libraries(vban_net PUBLIC vban config)

# One host test executable per component test/ directory.
file(GLOB TEST_DIRS CONFIGURE_DEPENDS ${COMP}/*/test)
foreach(dir ${TEST_DIRS})
    get_filename_component(comp ${dir} DIRECTORY)
    get_filename_component(comp ${comp} NAME)
    file(GLOB srcs CONFIGURE_DEPENDS ${dir}/*.c)
    add_executable(test_${comp} ${srcs})
    target_link_libraries(test_${comp} PRIVATE ${comp} unity unity_runner)
    add_test(NAME test_${comp} COMMAND test_${comp})
endforeach()

add_executable(test_kbd ${ROOT}/host/sim/test/test_kbd.c ${ROOT}/host/sim/sim_kbd.c)
target_include_directories(test_kbd PRIVATE ${ROOT}/host/sim)
target_link_libraries(test_kbd PRIVATE unity unity_runner)
add_test(NAME test_kbd COMMAND test_kbd)

find_package(Threads REQUIRED)
add_executable(
    vbankey-sim
    ${ROOT}/host/sim/main.c
    ${ROOT}/host/sim/sim_log.c
    ${ROOT}/host/sim/sim_exec.c
    ${ROOT}/host/sim/sim_kbd.c
)
target_include_directories(vbankey-sim PRIVATE ${ROOT}/host/sim)
target_link_libraries(vbankey-sim PRIVATE runtime vban_net Threads::Threads)
