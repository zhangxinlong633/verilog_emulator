if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs missing: ${VS}")
endif()

execute_process(
  COMMAND "${VS}" run "${SRC}"
          --until 20
          --watch m_1_0
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE RC)

if(NOT RC EQUAL 0)
  message(FATAL_ERROR "array2d_write failed rc=${RC}\n${ERR}\n${OUT}")
endif()

string(FIND "${OUT}" "m_1_0=5" POS)
if(POS EQUAL -1)
  message(FATAL_ERROR "expected m_1_0=5 in:\n${OUT}\nstderr:\n${ERR}")
endif()

message(STATUS "array2d_write OK: ${OUT}")
