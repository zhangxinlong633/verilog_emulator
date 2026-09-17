# Expects: VS, SRC, TRACE
# Checks that parameterized matmul meta expands @vs array= into cells a_0_0 style.
if(NOT VS OR NOT SRC OR NOT TRACE)
  message(FATAL_ERROR "VS, SRC, TRACE required")
endif()

execute_process(
  COMMAND ${VS} run ${SRC} --threads 1 --until 40
          --force a_0_0=1 --force a_0_1=2 --force a_1_0=3 --force a_1_1=4
          --force b_0_0=5 --force b_0_1=6 --force b_1_0=7 --force b_1_1=8
          --watch c_0_0 --trace ${TRACE}
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
string(FIND "${content}" "\"a_0_0\"" idx2)
if(idx2 EQUAL -1)
  message(FATAL_ERROR "meta views missing expanded cell a_0_0:\n${content}")
endif()
string(FIND "${content}" "\"c_1_1\"" idx3)
if(idx3 EQUAL -1)
  message(FATAL_ERROR "meta views missing expanded cell c_1_1:\n${content}")
endif()
string(FIND "${content}" "\"ops\"" idx4)
if(idx4 EQUAL -1)
  message(FATAL_ERROR "meta missing ops:\n${content}")
endif()
