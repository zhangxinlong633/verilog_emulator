if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs missing: ${VS}")
endif()

execute_process(
  COMMAND "${VS}" run "${SRC}"
          --until 10
          --watch w
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE RC)

if(RC EQUAL 0)
  message(FATAL_ERROR "gen_nolabel expected elab error, got:\n${OUT}")
endif()

string(FIND "${ERR}" "no block label" POS)
if(POS EQUAL -1)
  message(FATAL_ERROR "expected 'no block label' in:\n${ERR}\n${OUT}")
endif()

message(STATUS "gen_nolabel OK")
