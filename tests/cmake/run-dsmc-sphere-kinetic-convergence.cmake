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

set(test_name dsmc-sphere-kinetic-convergence)
set(test_root "${BINARY_DIR}/output/${test_name}")
set(template "${SOURCE_DIR}/tests/inputs/${test_name}/in.${test_name}.in")
set(cases_csv "${test_root}/cases.csv")
file(REMOVE_RECURSE "${test_root}")
file(MAKE_DIRECTORY "${test_root}")
file(WRITE "${cases_csv}" "resolution,history\n")

set(number_density "7.244e23")
set(temperature "5000.0")
set(molecular_mass "5.31352e-26")
set(solid_density "1800.0")
set(solid_molar_mass "0.0120107")
set(solid_atoms_per_hit "1.0")
set(initial_radius "5.0e-4")
set(ablation_time "2.144876345695438e-2")
set(mass_courant "0.3333333333333333")
set(sample_steps "10")
set(dsmc_dt "1.0e-7")
set(fnum "2.0e12")
set(domain_half_width "6.5e-4")
set(grid_cells "6")
set(surfmax "2500")
set(splitmax "250")
set(species_file "${SOURCE_DIR}/examples/dsmc-sphere-kinetic/air-react-to-flux.species")

foreach(resolution 4 6 8)
  set(case_dir "${test_root}/resolution-${resolution}")
  file(MAKE_DIRECTORY "${case_dir}")
  set(input_file "${case_dir}/in.${test_name}-${resolution}")
  set(history_file "${case_dir}/history.csv")
  configure_file("${template}" "${input_file}" @ONLY)
  execute_process(
    COMMAND "${DSMC_EXECUTABLE}" -screen none -log "${case_dir}/log.sparta" -in "${input_file}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )
  if(NOT result EQUAL 0)
    message(STATUS "${stdout}")
    message(STATUS "${stderr}")
    message(FATAL_ERROR "DSMC kinetic convergence case failed at resolution ${resolution}")
  endif()
  file(APPEND "${cases_csv}" "${resolution},${history_file}\n")
endforeach()

execute_process(
  COMMAND "${CHECKER}"
          --cases "${cases_csv}"
          --summary "${test_root}/summary.csv"
          --number-density "${number_density}"
          --temperature "${temperature}"
          --molecular-mass "${molecular_mass}"
          --solid-density "${solid_density}"
          --solid-molar-mass "${solid_molar_mass}"
          --solid-atoms-per-hit "${solid_atoms_per_hit}"
          --rad0 "${initial_radius}"
          --max-mass-error-percent 35.0
          --max-volume-error-percent 80.0
          --max-rad-error-percent 35.0
          --min-mass-improvement-percent 5.0
  RESULT_VARIABLE result
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr
)
message(STATUS "${stdout}")
if(stderr)
  message(STATUS "${stderr}")
endif()
if(NOT result EQUAL 0)
  message(FATAL_ERROR "DSMC kinetic convergence check failed")
endif()
