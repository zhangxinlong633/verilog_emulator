if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs missing: ${VS}")
endif()

execute_process(
  COMMAND "${VS}" run "${SRC}"
          --until 20
          --watch mem_3
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE RC)

if(NOT RC EQUAL 0)
  message(FATAL_ERROR "for_fill failed rc=${RC}\n${ERR}\n${OUT}")
endif()

string(FIND "${OUT}" "mem_3=3" POS)
if(POS EQUAL -1)
  message(FATAL_ERROR "expected mem_3=3 in:\n${OUT}\nstderr:\n${ERR}")
endif()

message(STATUS "for_fill OK: ${OUT}")
