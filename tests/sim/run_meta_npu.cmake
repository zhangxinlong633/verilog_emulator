# Expects: VS, SRC, BLAS_DIR, TRACE
if(NOT VS OR NOT SRC OR NOT TRACE OR NOT BLAS_DIR)
  message(FATAL_ERROR "VS, SRC, BLAS_DIR, TRACE required")
endif()

execute_process(
  COMMAND ${VS} run ${SRC}
          ${BLAS_DIR}/gemm.v
          ${BLAS_DIR}/gemm_bt.v
          ${BLAS_DIR}/relu.v
          ${BLAS_DIR}/row_argmax.v
          --threads 1 --until 400
          --clock clk=10 --reset rst=20
          --force x_0_0=1 --force x_0_1=2 --force x_1_0=3 --force x_1_1=4
          --force wq_0_0=1 --force wq_0_1=0 --force wq_1_0=0 --force wq_1_1=1
          --force wk_0_0=1 --force wk_0_1=0 --force wk_1_0=0 --force wk_1_1=1
          --force wv_0_0=1 --force wv_0_1=0 --force wv_1_0=0 --force wv_1_1=1
          --force w1_0_0=1 --force w1_0_1=0 --force w1_1_0=0 --force w1_1_1=1
          --force w2_0_0=1 --force w2_0_1=0 --force w2_1_0=0 --force w2_1_1=1
          --watch done,y_0_0 --trace ${TRACE}
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "vs run failed: ${err}\n${out}")
endif()

file(READ ${TRACE} content)
string(FIND "${content}" "\"views\"" idx)
if(idx EQUAL -1)
  message(FATAL_ERROR "meta missing views:\n${content}")
endif()
string(FIND "${content}" "\"x_0_0\"" idx2)
if(idx2 EQUAL -1)
  message(FATAL_ERROR "meta missing x_0_0 cell:\n${content}")
endif()
string(FIND "${content}" "\"y_1_1\"" idx3)
if(idx3 EQUAL -1)
  message(FATAL_ERROR "meta missing y_1_1 cell:\n${content}")
endif()
