if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs missing: ${VS}")
endif()

execute_process(
  COMMAND "${VS}" run "${SRC}"
          --threads 4
          --until 20
          --force a00=1 --force a01=2 --force a10=3 --force a11=4
          --force b00=5 --force b01=6 --force b10=7 --force b11=8
          --watch c00,c01,c10,c11,a00,a01,a10,a11,b00,b01,b10,b11
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE RC)

if(NOT RC EQUAL 0)
  message(FATAL_ERROR "matmul2x2 failed rc=${RC}\n${ERR}\n${OUT}")
endif()

# Expect [[19,22],[43,50]]
foreach(pair "c00=19" "c01=22" "c10=43" "c11=50")
  string(FIND "${OUT}" "${pair}" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "missing ${pair} in:\n${OUT}")
  endif()
endforeach()

message(STATUS "matmul2x2 OK: ${OUT}")
