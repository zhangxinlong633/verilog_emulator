cmake_minimum_required(VERSION 3.16)
project(vs C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

list(PREPEND CMAKE_PREFIX_PATH
  /opt/homebrew/opt/bison
  /usr/local/opt/bison)

find_package(BISON 3.0 REQUIRED)
find_package(FLEX REQUIRED)

message(STATUS "Bison: ${BISON_EXECUTABLE} (${BISON_VERSION})")
message(STATUS "Flex: ${FLEX_EXECUTABLE}")

BISON_TARGET(VsParse
  ${CMAKE_CURRENT_SOURCE_DIR}/src/front/parse.y
  ${CMAKE_CURRENT_BINARY_DIR}/parse.tab.c
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parse.tab.h
  COMPILE_FLAGS "-Wall")

FLEX_TARGET(VsLex
  ${CMAKE_CURRENT_SOURCE_DIR}/src/front/lex.l
  ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c)

ADD_FLEX_BISON_DEPENDENCY(VsLex VsParse)

add_library(vs_util STATIC
  src/util/arena.c
  src/util/strtab.c
  src/util/diag.c)
target_include_directories(vs_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(vs_front STATIC
  src/front/ast.c
  src/front/dump_ast.c
  src/front/parse_driver.c
  ${BISON_VsParse_OUTPUTS}
  ${FLEX_VsLex_OUTPUTS})
target_link_libraries(vs_front PUBLIC vs_util)
target_include_directories(vs_front PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(vs_front PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_executable(vs src/main.c)
target_link_libraries(vs PRIVATE vs_front)

add_executable(test_util tests/util/test_util.c)
target_link_libraries(test_util PRIVATE vs_util)

add_executable(test_ast_dump tests/front/test_ast_dump.c)
target_link_libraries(test_ast_dump PRIVATE vs_front)

enable_testing()
add_test(NAME util COMMAND test_util)
add_test(NAME ast_dump COMMAND test_ast_dump)

add_test(
  NAME parse.ok.empty_module
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/tests/parse/ok/empty_module.v
    -DEXP=${CMAKE_SOURCE_DIR}/tests/parse/ok/empty_module.ast.expected
    -P ${CMAKE_SOURCE_DIR}/tests/parse/run_ok.cmake)
