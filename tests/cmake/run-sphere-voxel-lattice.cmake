if(NOT DEFINED IA_CORE_EXECUTABLE)
  message(FATAL_ERROR "IA_CORE_EXECUTABLE is required")
endif()
if(NOT DEFINED DSMC_EXECUTABLE)
  message(FATAL_ERROR "DSMC_EXECUTABLE is required")
endif()
if(NOT DEFINED CHECKER)
  message(FATAL_ERROR "CHECKER is required")
endif()
if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()
if(NOT DEFINED BINARY_DIR)
  message(FATAL_ERROR "BINARY_DIR is required")
endif()

set(test_name sphere-voxel-lattice-verification)
set(test_root "${BINARY_DIR}/tests/run/${test_name}")
set(template "${SOURCE_DIR}/tests/inputs/sphere-voxel-lattice/in.sphere-voxel-lattice.verify.in")
file(REMOVE_RECURSE "${test_root}")
file(MAKE_DIRECTORY "${test_root}")

function(run_case case_name executable extra_args)
  set(case_dir "${test_root}/${case_name}")
  set(output_dir "${case_dir}/output")
  set(input_file "${case_dir}/in.sphere-voxel-lattice.verify")
  file(MAKE_DIRECTORY "${output_dir}")
  set(OUTPUT_DIR "${output_dir}")
  configure_file("${template}" "${input_file}" @ONLY)

  execute_process(
    COMMAND "${executable}" ${extra_args} -in "${input_file}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_FILE "${case_dir}/${case_name}.out"
    ERROR_FILE "${case_dir}/${case_name}.err"
    RESULT_VARIABLE result
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR
            "${case_name} sphere voxel lattice run failed with exit code ${result}; see ${case_dir}/${case_name}.out and ${case_dir}/${case_name}.err")
  endif()

  execute_process(
    COMMAND "${CHECKER}" "${output_dir}/voxels_000000.vtu" 45
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )
  if(stdout)
    message(STATUS "${stdout}")
  endif()
  if(stderr)
    message(STATUS "${stderr}")
  endif()
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "${case_name} sphere voxel lattice invariant check failed")
  endif()
endfunction()

run_case(standalone "${IA_CORE_EXECUTABLE}" "")
run_case(hosted "${DSMC_EXECUTABLE}" "-log;none")
