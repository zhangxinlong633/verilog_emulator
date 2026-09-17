if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs missing: ${VS}")
endif()

execute_process(
  COMMAND "${VS}" run "${SRC}"
          --threads 2
          --until 40
          --force a_0_0=1 --force a_0_1=2 --force a_1_0=3 --force a_1_1=4
          --force b_0_0=5 --force b_0_1=6 --force b_1_0=7 --force b_1_1=8
          --watch c_0_0,c_0_1,c_1_0,c_1_1
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE RC)

if(NOT RC EQUAL 0)
  message(FATAL_ERROR "blas_gemm failed rc=${RC}\n${ERR}\n${OUT}")
endif()

foreach(pair "c_0_0=19" "c_0_1=22" "c_1_0=43" "c_1_1=50")
  string(FIND "${OUT}" "${pair}" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "missing ${pair} in:\n${OUT}\nstderr:\n${ERR}")
  endif()
endforeach()

message(STATUS "blas_gemm OK: ${OUT}")
