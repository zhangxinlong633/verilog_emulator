if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs missing: ${VS}")
endif()

set(OUT1 "${CMAKE_BINARY_DIR}/watch1.txt")
set(OUT4 "${CMAKE_BINARY_DIR}/watch4.txt")

execute_process(
  COMMAND "${VS}" run "${SRC}" --threads 1 --until 200 --clock clk=10 --reset rst=20 --watch q,clk,rst
  OUTPUT_FILE "${OUT1}"
  ERROR_VARIABLE ERR1
  RESULT_VARIABLE RC1)
if(NOT RC1 EQUAL 0)
  message(FATAL_ERROR "threads=1 failed rc=${RC1}\n${ERR1}")
endif()

execute_process(
  COMMAND "${VS}" run "${SRC}" --threads 4 --until 200 --clock clk=10 --reset rst=20 --watch q,clk,rst
  OUTPUT_FILE "${OUT4}"
  ERROR_VARIABLE ERR4
  RESULT_VARIABLE RC4)
if(NOT RC4 EQUAL 0)
  message(FATAL_ERROR "threads=4 failed rc=${RC4}\n${ERR4}")
endif()

file(READ "${OUT1}" W1)
file(READ "${OUT4}" W4)
if(NOT W1 STREQUAL W4)
  message(FATAL_ERROR "determinism mismatch\nT1:${W1}\nT4:${W4}")
endif()

# Expect q counted up after reset (non-zero)
string(FIND "${W1}" "q=0" ZERO_POS)
# After 200 time with period 10 and reset 20, q should be > 0
string(REGEX MATCH "q=([0-9]+)" _ "${W1}")
if(NOT CMAKE_MATCH_1)
  message(FATAL_ERROR "no q= in output: ${W1}")
endif()
if(CMAKE_MATCH_1 STREQUAL "0")
  message(FATAL_ERROR "expected q > 0 after run, got: ${W1}")
endif()

message(STATUS "sim.counter.det OK: ${W1}")
