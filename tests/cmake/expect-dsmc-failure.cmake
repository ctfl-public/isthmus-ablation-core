if(NOT DEFINED DSMC_EXECUTABLE)
  message(FATAL_ERROR "DSMC_EXECUTABLE is required")
endif()
if(NOT DEFINED INPUT)
  message(FATAL_ERROR "INPUT is required")
endif()
if(NOT DEFINED EXPECTED_REGEX)
  message(FATAL_ERROR "EXPECTED_REGEX is required")
endif()

execute_process(
  COMMAND "${DSMC_EXECUTABLE}" -log none -in "${INPUT}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error_output
)

set(combined_output "${output}\n${error_output}")
if(result EQUAL 0)
  message(FATAL_ERROR "Expected DSMC command to fail, but it succeeded.\n${combined_output}")
endif()

string(REGEX MATCH "${EXPECTED_REGEX}" matched "${combined_output}")
if(NOT matched)
  message(FATAL_ERROR
      "Expected failed DSMC command output to match '${EXPECTED_REGEX}', but it did not.\n${combined_output}")
endif()

message(STATUS "Observed expected DSMC failure: ${EXPECTED_REGEX}")
