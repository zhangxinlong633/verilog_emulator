if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs binary missing: ${VS}")
endif()
execute_process(
  COMMAND "${VS}" --dump-ast "${SRC}"
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE RC)
if(RC EQUAL 0)
  message(FATAL_ERROR "expected failure but vs succeeded\nstdout:\n${OUT}")
endif()
file(READ "${EXP}" EXPECTED)
string(REPLACE "\n" ";" LINES "${EXPECTED}")
foreach(line IN LISTS LINES)
  if(line STREQUAL "")
    continue()
  endif()
  string(FIND "${ERR}" "${line}" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "diag missing substring '${line}'\nstderr:\n${ERR}")
  endif()
endforeach()
