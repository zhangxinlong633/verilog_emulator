if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs missing: ${VS}")
endif()
if(NOT SRC OR NOT CTRL OR NOT MAC)
  message(FATAL_ERROR "SRC CTRL MAC required")
endif()

execute_process(
  COMMAND "${VS}" run "${SRC}" "${CTRL}" "${MAC}"
          --threads 1
          --until 2000
          --clock clk=10
          --reset rst=20
          --watch done,c_0_0,c_0_1,c_0_7,c_1_0,c_3_3,c_4_0,c_4_4,c_7_7
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE RC)

if(NOT RC EQUAL 0)
  message(FATAL_ERROR "gemm8 tiled failed rc=${RC}\n${ERR}\n${OUT}")
endif()

foreach(pair
    "done=1"
    "c_0_0=1" "c_0_1=2" "c_0_7=8"
    "c_1_0=9" "c_3_3=28"
    "c_4_0=33" "c_4_4=37" "c_7_7=64")
  string(FIND "${OUT}" "${pair}" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "missing ${pair} in:\n${OUT}\nstderr:\n${ERR}")
  endif()
endforeach()

message(STATUS "gemm8 tiled OK: ${OUT}")
