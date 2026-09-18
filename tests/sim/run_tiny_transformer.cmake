if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs missing: ${VS}")
endif()
if(NOT SRC OR NOT BLAS_DIR OR NOT TINY_DIR)
  message(FATAL_ERROR "SRC BLAS_DIR TINY_DIR required")
endif()

file(STRINGS "${TINY_DIR}/forces.txt" FORCE_VALS)
set(FORCE_ARGS "")
foreach(fv ${FORCE_VALS})
  if(NOT fv STREQUAL "")
    list(APPEND FORCE_ARGS --force "${fv}")
  endif()
endforeach()

file(STRINGS "${TINY_DIR}/expect_y.inc" EXPECT_LINES)

set(WATCH "done,y_0_0,y_0_1,y_0_2,y_0_3,y_1_0,y_1_1,y_1_2,y_1_3,y_2_0,y_2_1,y_2_2,y_2_3,y_3_0,y_3_1,y_3_2,y_3_3")

execute_process(
  COMMAND "${VS}" run "${SRC}"
          "${TINY_DIR}/weights.v"
          "${BLAS_DIR}/gemm.v"
          "${BLAS_DIR}/gemm_bt.v"
          "${BLAS_DIR}/relu.v"
          "${BLAS_DIR}/row_argmax.v"
          --threads 1
          --until 400
          --clock clk=10
          --reset rst=20
          ${FORCE_ARGS}
          --watch ${WATCH}
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE RC)

if(NOT RC EQUAL 0)
  message(FATAL_ERROR "tiny_transformer failed rc=${RC}\n${ERR}\n${OUT}")
endif()

foreach(pair ${EXPECT_LINES})
  if(pair STREQUAL "")
    continue()
  endif()
  string(FIND "${OUT}" "${pair}" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "missing ${pair} in:\n${OUT}\nstderr:\n${ERR}")
  endif()
endforeach()

message(STATUS "tiny_transformer OK")
