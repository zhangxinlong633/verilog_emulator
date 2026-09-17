# Expects: VS, SRC, TRACE
if(NOT VS OR NOT SRC OR NOT TRACE)
  message(FATAL_ERROR "VS, SRC, TRACE required")
endif()

execute_process(
  COMMAND ${VS} run ${SRC} --threads 1 --until 40
          --force a=0 --watch y --trace ${TRACE}
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "vs run failed: ${err}\n${out}")
endif()

file(READ ${TRACE} content)
string(FIND "${content}" "\"views\"" idx)
if(idx EQUAL -1)
  message(FATAL_ERROR "meta missing views:\n${content}")
endif()
string(FIND "${content}" "\"exprs\"" idx2)
if(idx2 EQUAL -1)
  message(FATAL_ERROR "meta missing exprs:\n${content}")
endif()
