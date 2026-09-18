if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs missing: ${VS}")
endif()
if(NOT SRC OR NOT BLAS_DIR OR NOT EXPECT OR NOT WATCH)
  message(FATAL_ERROR "SRC BLAS_DIR EXPECT WATCH required")
endif()

file(STRINGS "${EXPECT}" EXPECT_LINES)

execute_process(
  COMMAND "${VS}" run "${SRC}"
          "${BLAS_DIR}/gemm.v"
          "${BLAS_DIR}/gemm_bt.v"
          "${BLAS_DIR}/relu.v"
          "${BLAS_DIR}/row_argmax.v"
          --threads 1
          --until 40
          --watch ${WATCH}
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE RC)

if(NOT RC EQUAL 0)
  message(FATAL_ERROR "demo failed rc=${RC}\n${ERR}\n${OUT}")
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

message(STATUS "demo OK: ${OUT}")
