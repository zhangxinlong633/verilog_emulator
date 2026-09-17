if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs missing: ${VS}")
endif()
if(NOT SRC)
  message(FATAL_ERROR "SRC required")
endif()

execute_process(
  COMMAND "${VS}" run "${SRC}"
          --threads 2
          --until 400
          --clock clk=10
          --reset rst=20
          --force x_0_0=1 --force x_0_1=2 --force x_1_0=3 --force x_1_1=4
          --force wq_0_0=1 --force wq_0_1=0 --force wq_1_0=0 --force wq_1_1=1
          --force wk_0_0=1 --force wk_0_1=0 --force wk_1_0=0 --force wk_1_1=1
          --force wv_0_0=1 --force wv_0_1=0 --force wv_1_0=0 --force wv_1_1=1
          --force w1_0_0=1 --force w1_0_1=0 --force w1_1_0=0 --force w1_1_1=1
          --force w2_0_0=1 --force w2_0_1=0 --force w2_1_0=0 --force w2_1_1=1
          --watch done,phase,y_0_0,y_0_1,y_1_0,y_1_1
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE RC)

if(NOT RC EQUAL 0)
  message(FATAL_ERROR "npu_transformer failed rc=${RC}\n${ERR}\n${OUT}")
endif()

foreach(pair "done=1" "y_0_0=3" "y_0_1=4" "y_1_0=3" "y_1_1=4")
  string(FIND "${OUT}" "${pair}" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "missing ${pair} in:\n${OUT}\nstderr:\n${ERR}")
  endif()
endforeach()

message(STATUS "npu_transformer OK: ${OUT}")
