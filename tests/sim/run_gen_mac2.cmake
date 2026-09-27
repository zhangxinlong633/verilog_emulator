if(NOT EXISTS "${VS}")
  message(FATAL_ERROR "vs missing: ${VS}")
endif()

execute_process(
  COMMAND "${VS}" run "${SRC}"
          --threads 1
          --until 20
          --force a_0=3 --force a_1=4
          --force b_0=5 --force b_1=6
          --watch y,g_0_p,g_1_p
  OUTPUT_VARIABLE OUT
  ERROR_VARIABLE ERR
  RESULT_VARIABLE RC)

if(NOT RC EQUAL 0)
  message(FATAL_ERROR "gen_mac2 failed rc=${RC}\n${ERR}\n${OUT}")
endif()

foreach(pair "y=39" "g_0_p=15" "g_1_p=24")
  string(FIND "${OUT}" "${pair}" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "missing ${pair} in:\n${OUT}\nstderr:\n${ERR}")
  endif()
endforeach()

message(STATUS "gen_mac2 OK: ${OUT}")
