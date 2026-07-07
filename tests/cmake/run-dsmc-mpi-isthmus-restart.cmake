if(NOT DEFINED DSMC_EXECUTABLE)
  message(FATAL_ERROR "DSMC_EXECUTABLE is required")
endif()
if(NOT DEFINED MPIEXEC_EXECUTABLE)
  message(FATAL_ERROR "MPIEXEC_EXECUTABLE is required")
endif()
if(NOT DEFINED MPIEXEC_NUMPROC_FLAG)
  message(FATAL_ERROR "MPIEXEC_NUMPROC_FLAG is required")
endif()
if(NOT DEFINED MPI_NP)
  message(FATAL_ERROR "MPI_NP is required")
endif()
if(NOT DEFINED BINARY_DIR)
  message(FATAL_ERROR "BINARY_DIR is required")
endif()

set(run_dir "${BINARY_DIR}/tests/run/dsmc-mpi-isthmus-restart")
file(REMOVE_RECURSE "${run_dir}")
file(MAKE_DIRECTORY "${run_dir}")

set(common_restart_globals
"seed                12345
global              nrho 1.0e20 fnum 1.0e10 gridcut 0.0 surfmax 2000 splitmax 200 &
                    nedgebadnum 20 ncutbadnum 20 nmismatch 20 comm/sort yes
")

file(WRITE "${run_dir}/in.write-restart" "${common_restart_globals}
dimension           3
timestep            1.0e-7

boundary            o o o
create_box          -8.0e-4 8.0e-4 -8.0e-4 8.0e-4 -8.0e-4 8.0e-4
create_grid         8 8 8
balance_grid        rcb cell

voxel_material      carbon density 1800.0 molar-mass 0.0120107 formula C
voxel_create        solid sphere diameter 8.0e-4 resolution 10 material carbon
isthmus_surface     skin voxels solid map yes
surf_install        skin particle none type 1
surf_collide        1 diffuse 300.0 1.0
surf_modify         all collide 1

write_restart       restart.isthmus
")

file(WRITE "${run_dir}/in.read-restart" "${common_restart_globals}
read_restart        restart.isthmus
")

file(WRITE "${run_dir}/in.read-restart-balance" "${common_restart_globals}
read_restart        restart.isthmus balance rcb cell
")

function(run_restart_step step_name input_file)
  execute_process(
    COMMAND "${MPIEXEC_EXECUTABLE}" "${MPIEXEC_NUMPROC_FLAG}" "${MPI_NP}"
            "${DSMC_EXECUTABLE}" -log none -in "${input_file}"
    WORKING_DIRECTORY "${run_dir}"
    OUTPUT_FILE "${run_dir}/${step_name}.out"
    ERROR_FILE "${run_dir}/${step_name}.err"
    RESULT_VARIABLE result
  )

  if(NOT result EQUAL 0)
    message(FATAL_ERROR
            "DSMC MPI ISTHMUS restart ${step_name} failed with exit code ${result}; see ${run_dir}/${step_name}.out and ${run_dir}/${step_name}.err")
  endif()
endfunction()

run_restart_step(write in.write-restart)
run_restart_step(read-same-rank in.read-restart)
run_restart_step(read-balance in.read-restart-balance)
