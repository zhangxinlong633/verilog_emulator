if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs missing: ${VS}")
endif()

execute_process(
  COMMAND "${VS}" run "${SRC}"
          --until 20
          --force x=7
          --watch y
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE RC)

if(NOT RC EQUAL 0)
  message(FATAL_ERROR "param_width failed rc=${RC}\n${ERR}\n${OUT}")
endif()

string(FIND "${OUT}" "y=7" POS)
if(POS EQUAL -1)
  message(FATAL_ERROR "expected y=7 in:\n${OUT}\nstderr:\n${ERR}")
endif()

message(STATUS "param_width OK: ${OUT}")
