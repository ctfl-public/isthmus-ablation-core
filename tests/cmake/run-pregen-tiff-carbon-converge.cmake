if(NOT DEFINED DSMC_EXECUTABLE)
  message(FATAL_ERROR "DSMC_EXECUTABLE is required")
endif()
if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()
if(NOT DEFINED BINARY_DIR)
  message(FATAL_ERROR "BINARY_DIR is required")
endif()
if(NOT DEFINED CARBON_TIFF)
  message(FATAL_ERROR "CARBON_TIFF is required")
endif()

set(run_dir "${BINARY_DIR}/tests/run/pregen-tiff-carbon-recession-dsmc-co-converge")
file(REMOVE_RECURSE "${run_dir}")
file(MAKE_DIRECTORY "${run_dir}")

foreach(file_name
    in.dsmc-co-converge
    air-react-to-flux.species
    carbon-co.surf)
  file(COPY
       "${SOURCE_DIR}/examples/pregen-tiff-carbon-recession/${file_name}"
       DESTINATION "${run_dir}")
endforeach()

file(COPY "${CARBON_TIFF}" DESTINATION "${run_dir}")
file(COPY
     "${SOURCE_DIR}/tests/inputs/pregen-tiff-carbon-recession/in.dsmc-co-converge.verify.in"
     DESTINATION "${run_dir}")
file(RENAME
     "${run_dir}/in.dsmc-co-converge.verify.in"
     "${run_dir}/in.dsmc-co-converge.verify")

execute_process(
  COMMAND "${DSMC_EXECUTABLE}" -screen none -log none -in in.dsmc-co-converge.verify
  WORKING_DIRECTORY "${run_dir}"
  RESULT_VARIABLE result
)

if(NOT result EQUAL 0)
  message(FATAL_ERROR "pregen TIFF converged mass-flux verification failed with exit code ${result}")
endif()
