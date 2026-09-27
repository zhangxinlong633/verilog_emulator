if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs missing: ${VS}")
endif()

execute_process(
  COMMAND "${VS}" run "${SRC}"
          --threads 1
          --until 20
          --force a_0=9 --force a_1=8
          --watch y_0,y_1
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE RC)

if(NOT RC EQUAL 0)
  message(FATAL_ERROR "gen_buf2 failed rc=${RC}\n${ERR}\n${OUT}")
endif()

foreach(pair "y_0=9" "y_1=8")
  string(FIND "${OUT}" "${pair}" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "missing ${pair} in:\n${OUT}\nstderr:\n${ERR}")
  endif()
endforeach()

message(STATUS "gen_buf2 OK: ${OUT}")
