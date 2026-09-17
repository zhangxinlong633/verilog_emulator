cmake_minimum_required(VERSION 3.16)
project(vs C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(vs_util STATIC
  src/util/arena.c
  src/util/strtab.c
  src/util/diag.c)
target_include_directories(vs_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(vs_front STATIC
  src/front/ast.c
  src/front/dump_ast.c)
target_link_libraries(vs_front PUBLIC vs_util)
target_include_directories(vs_front PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(vs src/main.c)
target_link_libraries(vs PRIVATE vs_front)

add_executable(test_util tests/util/test_util.c)
target_link_libraries(test_util PRIVATE vs_util)

add_executable(test_ast_dump tests/front/test_ast_dump.c)
target_link_libraries(test_ast_dump PRIVATE vs_front)

enable_testing()
add_test(NAME util COMMAND test_util)
add_test(NAME ast_dump COMMAND test_ast_dump)
