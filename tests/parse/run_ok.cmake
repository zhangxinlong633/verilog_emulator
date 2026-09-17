if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs binary missing: ${VS}")
endif()
execute_process(
  COMMAND "${VS}" --dump-ast "${SRC}"
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE RC)
if(NOT RC EQUAL 0)
  message(FATAL_ERROR "vs failed rc=${RC}\nstderr:\n${ERR}\nstdout:\n${OUT}")
endif()
file(READ "${EXP}" EXPECTED)
if(NOT OUT STREQUAL EXPECTED)
  message(FATAL_ERROR "AST mismatch\nExpected:\n${EXPECTED}\nGot:\n${OUT}")
endif()
