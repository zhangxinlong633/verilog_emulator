if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs missing: ${VS}")
endif()

execute_process(
  COMMAND "${VS}" run "${SRC}"
          --until 20
          --force a=3
          --watch y
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE RC)

if(NOT RC EQUAL 0)
  message(FATAL_ERROR "inst_leaf failed rc=${RC}\n${ERR}\n${OUT}")
endif()

string(FIND "${OUT}" "y=3" POS)
if(POS EQUAL -1)
  message(FATAL_ERROR "expected y=3 in:\n${OUT}\nstderr:\n${ERR}")
endif()

message(STATUS "inst_leaf OK: ${OUT}")
