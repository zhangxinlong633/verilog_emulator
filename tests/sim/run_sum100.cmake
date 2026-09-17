if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs missing: ${VS}")
endif()

# Need enough time: reset 20, then >= 101 posedges (period 10) => until ~1200
execute_process(
  COMMAND "${VS}" run "${SRC}"
          --threads 4
          --until 1200
          --clock clk=10
          --reset rst=20
          --watch sum,i,done,clk,rst
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE RC)

if(NOT RC EQUAL 0)
  message(FATAL_ERROR "sum_1_to_100 failed rc=${RC}\n${ERR}\n${OUT}")
endif()

string(REGEX MATCH "sum=([0-9]+)" _ "${OUT}")
set(SUM "${CMAKE_MATCH_1}")
string(REGEX MATCH "done=([0-9]+)" _ "${OUT}")
set(DONE "${CMAKE_MATCH_1}")

if(NOT SUM STREQUAL "5050")
  message(FATAL_ERROR "expected sum=5050, got output:\n${OUT}")
endif()
if(NOT DONE STREQUAL "1")
  message(FATAL_ERROR "expected done=1, got output:\n${OUT}")
endif()

message(STATUS "sum_1_to_100 OK: ${OUT}")
