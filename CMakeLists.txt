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
find_package(Threads REQUIRED)

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
  src/util/diag.c
  src/util/anno.c)
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

add_library(vs_elab STATIC src/elab/elab.c)
target_link_libraries(vs_elab PUBLIC vs_front)

add_library(vs_sim STATIC src/sim/sim.c)
target_link_libraries(vs_sim PUBLIC vs_elab Threads::Threads)

add_executable(vs src/main.c)
target_link_libraries(vs PRIVATE vs_sim)

add_executable(test_util tests/util/test_util.c)
target_link_libraries(test_util PRIVATE vs_util)

add_executable(test_anno tests/util/test_anno.c)
target_link_libraries(test_anno PRIVATE vs_util)

add_executable(test_ast_dump tests/front/test_ast_dump.c)
target_link_libraries(test_ast_dump PRIVATE vs_front)

enable_testing()
add_test(NAME util COMMAND test_util)
add_test(NAME anno COMMAND test_anno)
set_tests_properties(anno PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME ast_dump COMMAND test_ast_dump)

add_test(
  NAME parse.ok.empty_module
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/tests/parse/ok/empty_module.v
    -DEXP=${CMAKE_SOURCE_DIR}/tests/parse/ok/empty_module.ast.expected
    -P ${CMAKE_SOURCE_DIR}/tests/parse/run_ok.cmake)

add_test(
  NAME parse.ok.inv
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/tests/parse/ok/inv.v
    -DEXP=${CMAKE_SOURCE_DIR}/tests/parse/ok/inv.ast.expected
    -P ${CMAKE_SOURCE_DIR}/tests/parse/run_ok.cmake)

add_test(
  NAME parse.ok.param_width
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/tests/parse/ok/param_width.v
    -DEXP=${CMAKE_SOURCE_DIR}/tests/parse/ok/param_width.ast.expected
    -P ${CMAKE_SOURCE_DIR}/tests/parse/run_ok.cmake)

add_test(
  NAME parse.ok.for_fill
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/tests/parse/ok/for_fill.v
    -DEXP=${CMAKE_SOURCE_DIR}/tests/parse/ok/for_fill.ast.expected
    -P ${CMAKE_SOURCE_DIR}/tests/parse/run_ok.cmake)

add_test(
  NAME parse.ok.counter
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/tests/parse/ok/counter.v
    -DEXP=${CMAKE_SOURCE_DIR}/tests/parse/ok/counter.ast.expected
    -P ${CMAKE_SOURCE_DIR}/tests/parse/run_ok.cmake)

add_test(
  NAME parse.ok.inst_gemm
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/tests/parse/ok/inst_gemm.v
    -DEXP=${CMAKE_SOURCE_DIR}/tests/parse/ok/inst_gemm.ast.expected
    -P ${CMAKE_SOURCE_DIR}/tests/parse/run_ok.cmake)

add_test(
  NAME parse.fail.missing_endmodule
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/tests/parse/fail/missing_endmodule.v
    -DEXP=${CMAKE_SOURCE_DIR}/tests/parse/fail/missing_endmodule.diag.expected
    -P ${CMAKE_SOURCE_DIR}/tests/parse/run_fail.cmake)

add_test(
  NAME parse.fail.bad_assign
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/tests/parse/fail/bad_assign.v
    -DEXP=${CMAKE_SOURCE_DIR}/tests/parse/fail/bad_assign.diag.expected
    -P ${CMAKE_SOURCE_DIR}/tests/parse/run_fail.cmake)

add_test(
  NAME sim.counter.det
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/examples/counter.v
    -P ${CMAKE_SOURCE_DIR}/tests/sim/run_det.cmake)

add_test(
  NAME sim.sum_1_to_100
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/examples/sum_1_to_100.v
    -P ${CMAKE_SOURCE_DIR}/tests/sim/run_sum100.cmake)

add_test(
  NAME sim.matmul2x2
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/examples/matmul2x2.v
    -P ${CMAKE_SOURCE_DIR}/tests/sim/run_matmul.cmake)

add_test(
  NAME sim.matmul_param
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/examples/matmul.v
    -P ${CMAKE_SOURCE_DIR}/tests/sim/run_matmul_param.cmake)

add_test(
  NAME sim.blas_gemm
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/examples/blas/gemm.v
    -P ${CMAKE_SOURCE_DIR}/tests/sim/run_blas_gemm.cmake)

add_test(
  NAME sim.npu_transformer
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/examples/npu_transformer.v
    -DBLAS_DIR=${CMAKE_SOURCE_DIR}/examples/blas
    -P ${CMAKE_SOURCE_DIR}/tests/sim/run_npu_transformer.cmake)

add_test(
  NAME sim.tiny_transformer
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/examples/tiny_npu/tiny_transformer.v
    -DBLAS_DIR=${CMAKE_SOURCE_DIR}/examples/blas
    -DTINY_DIR=${CMAKE_SOURCE_DIR}/examples/tiny_npu
    -P ${CMAKE_SOURCE_DIR}/tests/sim/run_tiny_transformer.cmake)

add_test(
  NAME sim.tiny_demo_gemm
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/examples/tiny_npu/demo_gemm.v
    -DBLAS_DIR=${CMAKE_SOURCE_DIR}/examples/blas
    -DEXPECT=${CMAKE_SOURCE_DIR}/examples/tiny_npu/expect_gemm.inc
    -DWATCH=c_0_0,c_0_1,c_0_2,c_0_3,c_1_0,c_1_1,c_1_2,c_1_3,c_2_0,c_2_1,c_2_2,c_2_3,c_3_0,c_3_1,c_3_2,c_3_3
    -P ${CMAKE_SOURCE_DIR}/tests/sim/run_tiny_demo.cmake)

add_test(
  NAME sim.tiny_demo_mlp
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/examples/tiny_npu/demo_mlp.v
    -DBLAS_DIR=${CMAKE_SOURCE_DIR}/examples/blas
    -DEXPECT=${CMAKE_SOURCE_DIR}/examples/tiny_npu/expect_mlp.inc
    -DWATCH=y_0_0,y_0_1,y_0_2,y_0_3,y_1_0,y_1_1,y_1_2,y_1_3,y_2_0,y_2_1,y_2_2,y_2_3,y_3_0,y_3_1,y_3_2,y_3_3
    -P ${CMAKE_SOURCE_DIR}/tests/sim/run_tiny_demo.cmake)

add_test(
  NAME sim.tiny_demo_attn
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/examples/tiny_npu/demo_attn.v
    -DBLAS_DIR=${CMAKE_SOURCE_DIR}/examples/blas
    -DEXPECT=${CMAKE_SOURCE_DIR}/examples/tiny_npu/expect_attn.inc
    -DWATCH=o_0_0,o_0_1,o_0_2,o_0_3,o_1_0,o_1_1,o_1_2,o_1_3,o_2_0,o_2_1,o_2_2,o_2_3,o_3_0,o_3_1,o_3_2,o_3_3
    -P ${CMAKE_SOURCE_DIR}/tests/sim/run_tiny_demo.cmake)

add_test(
  NAME sim.meta_npu
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/examples/npu_transformer.v
    -DBLAS_DIR=${CMAKE_SOURCE_DIR}/examples/blas
    -DTRACE=${CMAKE_BINARY_DIR}/trace_meta_npu.jsonl
    -P ${CMAKE_SOURCE_DIR}/tests/sim/run_meta_npu.cmake)

add_test(
  NAME sim.meta_views
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/tests/sim/fixtures/inv_anno.v
    -DTRACE=${CMAKE_BINARY_DIR}/trace_meta_views.jsonl
    -P ${CMAKE_SOURCE_DIR}/tests/sim/run_meta_views.cmake)

add_test(
  NAME sim.meta_matmul
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/examples/matmul.v
    -DTRACE=${CMAKE_BINARY_DIR}/trace_meta_matmul.jsonl
    -P ${CMAKE_SOURCE_DIR}/tests/sim/run_meta_matmul.cmake)

add_test(
  NAME sim.param_width
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/tests/sim/fixtures/param_width.v
    -P ${CMAKE_SOURCE_DIR}/tests/sim/run_param_width.cmake)

add_test(
  NAME sim.inst_leaf
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/tests/sim/inst_leaf.v
    -P ${CMAKE_SOURCE_DIR}/tests/sim/run_inst_leaf.cmake)

add_test(
  NAME sim.for_fill
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/tests/sim/fixtures/for_fill.v
    -P ${CMAKE_SOURCE_DIR}/tests/sim/run_for_fill.cmake)

add_test(
  NAME sim.array2d_write
  COMMAND ${CMAKE_COMMAND}
    -DVS=$<TARGET_FILE:vs>
    -DSRC=${CMAKE_SOURCE_DIR}/tests/sim/fixtures/array2d_write.v
    -P ${CMAKE_SOURCE_DIR}/tests/sim/run_array2d_write.cmake)
