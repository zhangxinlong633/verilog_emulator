if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs missing: ${VS}")
endif()
if(NOT SRC OR NOT BLAS_DIR OR NOT EXPECT OR NOT WATCH_FILE)
  message(FATAL_ERROR "SRC BLAS_DIR EXPECT WATCH_FILE required")
endif()

file(STRINGS "${EXPECT}" EXPECT_LINES)
file(READ "${WATCH_FILE}" WATCH)
string(STRIP "${WATCH}" WATCH)

execute_process(
  COMMAND "${VS}" run "${SRC}"
          "${BLAS_DIR}/gemm.v"
          --threads 1
          --until 40
          --watch ${WATCH}
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE RC)

if(NOT RC EQUAL 0)
  message(FATAL_ERROR "gpt2_npu_gemm failed rc=${RC}\n${ERR}\n${OUT}")
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

message(STATUS "gpt2_npu_gemm OK")
