if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs missing: ${VS}")
endif()
if(NOT SRC OR NOT FSM OR NOT MAC)
  message(FATAL_ERROR "SRC FSM MAC required")
endif()

execute_process(
  COMMAND "${VS}" run "${SRC}" "${FSM}" "${MAC}"
          --threads 1
          --until 800
          --clock clk=10
          --reset rst=20
          --watch done,c_0_0,c_0_1,c_0_2,c_0_3,c_1_0,c_1_1,c_1_2,c_1_3,c_2_0,c_2_1,c_2_2,c_2_3,c_3_0,c_3_1,c_3_2,c_3_3
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE RC)

if(NOT RC EQUAL 0)
  message(FATAL_ERROR "gemm3 failed rc=${RC}\n${ERR}\n${OUT}")
endif()

# A=I, B=1..16 → C=B
foreach(pair
    "done=1"
    "c_0_0=1" "c_0_1=2" "c_0_2=3" "c_0_3=4"
    "c_1_0=5" "c_1_1=6" "c_1_2=7" "c_1_3=8"
    "c_2_0=9" "c_2_1=10" "c_2_2=11" "c_2_3=12"
    "c_3_0=13" "c_3_1=14" "c_3_2=15" "c_3_3=16")
  string(FIND "${OUT}" "${pair}" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "missing ${pair} in:\n${OUT}\nstderr:\n${ERR}")
  endif()
endforeach()

message(STATUS "gemm3 OK: ${OUT}")
