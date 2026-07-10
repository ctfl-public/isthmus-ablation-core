cmake_minimum_required(VERSION 3.16)

project(isthmus_ablation_core LANGUAGES CXX)

include(CheckLibraryExists)
include(CheckCXXSourceCompiles)

function(iac_first_existing_dir out_var)
  foreach(candidate IN LISTS ARGN)
    if(candidate AND EXISTS "${candidate}")
      set(${out_var} "${candidate}" PARENT_SCOPE)
      return()
    endif()
  endforeach()
  set(${out_var} "" PARENT_SCOPE)
endfunction()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(isthmus_ablation_core
  src/expression.cpp
  src/model.cpp
  src/parser.cpp
)

target_include_directories(isthmus_ablation_core PUBLIC include)

set(ISTHMUS_ROOT "" CACHE PATH "Root directory of an ISTHMUS checkout/build")
if(NOT ISTHMUS_ROOT AND DEFINED ENV{ISTHMUS_ROOT})
  set(ISTHMUS_ROOT "$ENV{ISTHMUS_ROOT}" CACHE PATH "Root directory of an ISTHMUS checkout/build" FORCE)
endif()

set(_iac_isthmus_package_paths
  "${ISTHMUS_ROOT}"
  "${ISTHMUS_ROOT}/build"
  "${ISTHMUS_ROOT}/build-wsl"
  "$ENV{ISTHMUS_CPP_DIR}"
  "$ENV{isthmus_cpp_DIR}"
  "${CMAKE_SOURCE_DIR}/../isthmus/build"
  "${CMAKE_SOURCE_DIR}/../isthmus/build-wsl"
  "$ENV{HOME}/isthmus/build"
  "$ENV{HOME}/isthmus/build-wsl"
)

find_package(isthmus_cpp CONFIG REQUIRED PATHS
  ${_iac_isthmus_package_paths}
)

target_link_libraries(isthmus_ablation_core PUBLIC isthmus_cpp::isthmus_cpp)
target_compile_definitions(isthmus_ablation_core PRIVATE IAC_HAS_ISTHMUS=1)
get_target_property(IAC_ISTHMUS_INCLUDE_DIRS isthmus_cpp::isthmus_cpp INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(IAC_ISTHMUS_LIBRARY isthmus_cpp::isthmus_cpp IMPORTED_LOCATION)
if(NOT IAC_ISTHMUS_LIBRARY)
  get_target_property(IAC_ISTHMUS_LIBRARY isthmus_cpp::isthmus_cpp IMPORTED_LOCATION_NOCONFIG)
endif()
if((NOT IAC_ISTHMUS_LIBRARY OR IAC_ISTHMUS_LIBRARY MATCHES "-NOTFOUND$") AND
   EXISTS "${ISTHMUS_ROOT}/libisthmus_cpp.a")
  set(IAC_ISTHMUS_LIBRARY "${ISTHMUS_ROOT}/libisthmus_cpp.a")
endif()

set(_iac_try_compile_target_type_was_defined FALSE)
if(DEFINED CMAKE_TRY_COMPILE_TARGET_TYPE)
  set(_iac_try_compile_target_type_was_defined TRUE)
  set(_iac_try_compile_target_type "${CMAKE_TRY_COMPILE_TARGET_TYPE}")
endif()
set(_iac_required_libraries "${CMAKE_REQUIRED_LIBRARIES}")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_REQUIRED_INCLUDES ${IAC_ISTHMUS_INCLUDE_DIRS})
unset(IAC_ISTHMUS_HAS_RUN_OPTIONS_DOMAIN CACHE)
check_cxx_source_compiles([=[
  #include <isthmus/marching_windows.hpp>

  int main() {
    isthmus::RunOptions options;
    options.dimension = isthmus::Dimension::D3;
    options.voxel_size = 1.0;
    options.weighting = true;
    options.iso_value = 0.5;
    options.marching_voxel_ratio = 1.0;
    isthmus::VoxelSet voxels;
    isthmus::MarchingWindows marching;
    auto result = marching.run(voxels, options);
    (void)result;
    return 0;
  }
]=] IAC_ISTHMUS_HAS_RUN_OPTIONS_DOMAIN)

if(_iac_try_compile_target_type_was_defined)
  set(CMAKE_TRY_COMPILE_TARGET_TYPE "${_iac_try_compile_target_type}")
else()
  unset(CMAKE_TRY_COMPILE_TARGET_TYPE)
endif()
set(CMAKE_REQUIRED_LIBRARIES isthmus_cpp::isthmus_cpp)
if(IAC_ISTHMUS_HAS_RUN_OPTIONS_DOMAIN)
  unset(IAC_ISTHMUS_LINKS_RUN_OPTIONS_DOMAIN CACHE)
  check_cxx_source_compiles([=[
    #include <isthmus/marching_windows.hpp>

    int main() {
      isthmus::RunOptions options;
      options.dimension = isthmus::Dimension::D3;
      options.voxel_size = 1.0;
      options.weighting = true;
      options.iso_value = 0.5;
      options.marching_voxel_ratio = 1.0;
      isthmus::VoxelSet voxels;
      isthmus::MarchingWindows marching;
      auto result = marching.run(voxels, options);
      (void)result;
      return 0;
    }
  ]=] IAC_ISTHMUS_LINKS_RUN_OPTIONS_DOMAIN)
  if(NOT IAC_ISTHMUS_LINKS_RUN_OPTIONS_DOMAIN)
    message(FATAL_ERROR
      "ISTHMUS headers expose the RunOptions-derived MarchingWindows API, "
      "but the linked ISTHMUS library does not. Rebuild ISTHMUS after switching "
      "branches, or point ISTHMUS_ROOT at a matching build directory.")
  endif()
else()
  unset(IAC_ISTHMUS_LINKS_EXPLICIT_DOMAIN CACHE)
  check_cxx_source_compiles([=[
    #include <isthmus/marching_windows.hpp>

    int main() {
      isthmus::DomainConfig domain;
      isthmus::VoxelSet voxels;
      isthmus::RunOptions options;
      isthmus::MarchingWindows marching;
      auto result = marching.run(domain, voxels, options);
      (void)result;
      return 0;
    }
  ]=] IAC_ISTHMUS_LINKS_EXPLICIT_DOMAIN)
  if(NOT IAC_ISTHMUS_LINKS_EXPLICIT_DOMAIN)
    message(FATAL_ERROR
      "ISTHMUS headers expose the explicit-DomainConfig MarchingWindows API, "
      "but the linked ISTHMUS library does not. Rebuild ISTHMUS after switching "
      "branches, or point ISTHMUS_ROOT at a matching build directory.")
  endif()
endif()
set(CMAKE_REQUIRED_INCLUDES)
set(CMAKE_REQUIRED_LIBRARIES "${_iac_required_libraries}")
if(_iac_try_compile_target_type_was_defined)
  set(CMAKE_TRY_COMPILE_TARGET_TYPE "${_iac_try_compile_target_type}")
else()
  unset(CMAKE_TRY_COMPILE_TARGET_TYPE)
endif()
if(IAC_ISTHMUS_HAS_RUN_OPTIONS_DOMAIN)
  target_compile_definitions(isthmus_ablation_core PRIVATE IAC_ISTHMUS_RUN_OPTIONS_DOMAIN=1)
  message(STATUS "ISTHMUS API: RunOptions-derived marching domain")
else()
  message(STATUS "ISTHMUS API: explicit DomainConfig marching domain")
endif()

if(MSVC)
  target_compile_options(isthmus_ablation_core PRIVATE /W4)
else()
  target_compile_options(isthmus_ablation_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(ia-core src/main.cpp)
target_link_libraries(ia-core PRIVATE isthmus_ablation_core)

add_executable(
  dsmc-kinetic-convergence-check
  tests/tools/dsmc-kinetic-convergence-check.cpp
)

add_executable(
  create-dsmc-overlay
  tools/create_dsmc_overlay.cpp
)

find_package(Python3 COMPONENTS Interpreter)
find_package(MPI QUIET)
find_program(PDFLATEX_EXECUTABLE pdflatex)
find_program(IAC_MAKE_EXECUTABLE NAMES make gmake)

if(Python3_Interpreter_FOUND)
  set(IAC_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
  set(IAC_CARBON_SAMPLE_TIFF "${IAC_GENERATED_DIR}/carbon-sample.tif")
  set(IAC_MULTIPHASE_SLAB_TIFF "${IAC_GENERATED_DIR}/multiphase-slab-labels.tif")
  add_custom_command(
    OUTPUT ${IAC_CARBON_SAMPLE_TIFF}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${IAC_GENERATED_DIR}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/make-carbon-sample-tiff.py
            --width 24 --height 24 --depth 10 --out ${IAC_CARBON_SAMPLE_TIFF}
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/make-carbon-sample-tiff.py
    COMMENT "Generating deterministic carbon-sample TIFF fixture"
  )
  add_custom_command(
    OUTPUT ${IAC_MULTIPHASE_SLAB_TIFF}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${IAC_GENERATED_DIR}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/make-multiphase-slab-tiff.py
            --width 4 --height 4 --depth 2 --out ${IAC_MULTIPHASE_SLAB_TIFF}
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/make-multiphase-slab-tiff.py
    COMMENT "Generating deterministic multiphase slab TIFF fixture"
  )
  add_custom_target(generated-fixtures DEPENDS
    ${IAC_CARBON_SAMPLE_TIFF}
    ${IAC_MULTIPHASE_SLAB_TIFF}
  )
  message(STATUS "Generated TIFF fixture tests: enabled with ${Python3_EXECUTABLE}")
else()
  message(STATUS "Generated TIFF fixture tests: disabled because Python3 was not found")
endif()

set(DSMC_ROOT "" CACHE PATH "Optional root directory of a DSMC/SPARTA source checkout")
if(NOT DSMC_ROOT AND DEFINED ENV{DSMC_ROOT})
  set(DSMC_ROOT "$ENV{DSMC_ROOT}" CACHE PATH "Optional root directory of a DSMC/SPARTA source checkout" FORCE)
endif()
if(NOT DSMC_ROOT)
  iac_first_existing_dir(DSMC_ROOT
    "${CMAKE_SOURCE_DIR}/../dsmc"
    "$ENV{HOME}/dsmc"
  )
  if(DSMC_ROOT)
    set(DSMC_ROOT "${DSMC_ROOT}" CACHE PATH "Optional root directory of a DSMC/SPARTA source checkout" FORCE)
  endif()
endif()

set(DSMC_MACHINE "mpi" CACHE STRING "DSMC make target used by the IAC overlay build")
set(IAC_DSMC_MPI_NP "2" CACHE STRING "MPI rank count used by DSMC/IAC MPI tests")
set(IAC_MPIEXEC_EXECUTABLE "${MPIEXEC_EXECUTABLE}" CACHE FILEPATH "MPI launcher used for DSMC/IAC MPI tests")
set(IAC_MPIEXEC_NUMPROC_FLAG "${MPIEXEC_NUMPROC_FLAG}" CACHE STRING "MPI launcher flag for rank count")
option(IAC_ENABLE_MPI_TESTS "Enable DSMC/IAC MPI CTest entries when an MPI launcher is available" ON)
set(IAC_DSMC_OVERLAY_SRC "${CMAKE_BINARY_DIR}/dsmc-overlay/src")
set(IAC_DSMC_OVERLAY_EXECUTABLE "${IAC_DSMC_OVERLAY_SRC}/spa_${DSMC_MACHINE}")
option(IAC_DSMC_USE_OVERLAY "Use the private DSMC overlay executable for coupled tests" ON)
set(IAC_DSMC_EXECUTABLE "" CACHE FILEPATH "Optional DSMC/SPARTA executable for coupled tests")
if(IAC_DSMC_USE_OVERLAY AND DSMC_ROOT)
  set(IAC_DSMC_EXECUTABLE "${CMAKE_BINARY_DIR}/bin/dsmc-iac" CACHE FILEPATH
      "Optional DSMC/SPARTA executable for coupled tests" FORCE)
endif()

if(IAC_DSMC_USE_OVERLAY AND DSMC_ROOT)
  add_executable(dsmc-iac src/dsmc_iac_launcher.cpp)
  set_target_properties(dsmc-iac PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )
  target_compile_definitions(dsmc-iac PRIVATE
    IAC_DSMC_EXECUTABLE_PATH="${IAC_DSMC_OVERLAY_EXECUTABLE}"
  )
  check_library_exists(util forkpty "" IAC_DSMC_LAUNCHER_NEEDS_UTIL)
  if(IAC_DSMC_LAUNCHER_NEEDS_UTIL)
    target_link_libraries(dsmc-iac PRIVATE util)
  endif()
endif()

set(IAC_DSMC_REQUIRED_MASS_FLUX_FILES
  src/compute_react_surf_mass_flux.cpp
  src/compute_react_surf_mass_flux.h
  src/compute_react_boundary_mass_flux.cpp
  src/compute_react_boundary_mass_flux.h
)
if(IAC_DSMC_USE_OVERLAY AND DSMC_ROOT)
  set(IAC_DSMC_MISSING_MASS_FLUX_FILES)
  foreach(_iac_dsmc_required_file IN LISTS IAC_DSMC_REQUIRED_MASS_FLUX_FILES)
    if(NOT EXISTS "${DSMC_ROOT}/${_iac_dsmc_required_file}")
      list(APPEND IAC_DSMC_MISSING_MASS_FLUX_FILES
        "${DSMC_ROOT}/${_iac_dsmc_required_file}")
    endif()
  endforeach()
  if(IAC_DSMC_MISSING_MASS_FLUX_FILES)
    string(REPLACE ";" "\n  " _iac_dsmc_missing_message
           "${IAC_DSMC_MISSING_MASS_FLUX_FILES}")
    message(FATAL_ERROR
      "DSMC/IAC linked builds require DSMC direct reaction mass-flux support.\n"
      "Update DSMC to a branch containing compute react/surf/mass/flux and "
      "compute react/boundary/mass/flux, or configure standalone with "
      "IAC_DSMC_USE_OVERLAY=OFF.\n"
      "Missing files:\n  ${_iac_dsmc_missing_message}")
  endif()
endif()

set(IAC_DSMC_OVERLAY_ISTHMUS_INCLUDE_ARGS)
if(IAC_ISTHMUS_INCLUDE_DIRS)
  foreach(_iac_isthmus_include IN LISTS IAC_ISTHMUS_INCLUDE_DIRS)
    list(APPEND IAC_DSMC_OVERLAY_ISTHMUS_INCLUDE_ARGS
      --isthmus-include "${_iac_isthmus_include}")
  endforeach()
endif()

set(IAC_DSMC_TARGET_DEPENDS ia-core dsmc-kinetic-convergence-check)
if(TARGET dsmc-iac)
  list(APPEND IAC_DSMC_TARGET_DEPENDS dsmc-iac)
endif()
if(TARGET create-dsmc-overlay)
  list(APPEND IAC_DSMC_TARGET_DEPENDS create-dsmc-overlay)
endif()
if(IAC_DSMC_USE_OVERLAY AND DSMC_ROOT AND IAC_MAKE_EXECUTABLE)
  add_custom_target(
    dsmc
    COMMAND $<TARGET_FILE:create-dsmc-overlay>
            --dsmc-root ${DSMC_ROOT}
            --bridge-dir ${CMAKE_SOURCE_DIR}/dsmc-bridge
            --overlay-src ${IAC_DSMC_OVERLAY_SRC}
            --iac-include ${CMAKE_SOURCE_DIR}/include
            --iac-lib $<TARGET_FILE:isthmus_ablation_core>
            --isthmus-lib ${IAC_ISTHMUS_LIBRARY}
            ${IAC_DSMC_OVERLAY_ISTHMUS_INCLUDE_ARGS}
    COMMAND ${CMAKE_COMMAND} -E echo "[IAC build] Entering DSMC native make target '${DSMC_MACHINE}'."
    COMMAND ${CMAKE_COMMAND} -E echo "[IAC build] DSMC compiles many objects, then performs a large final link."
    COMMAND ${CMAKE_COMMAND} -E echo "[IAC build] The final link may be quiet for several minutes on shared filesystems."
    COMMAND ${CMAKE_COMMAND} -E chdir ${IAC_DSMC_OVERLAY_SRC}
            ${IAC_MAKE_EXECUTABLE} ${DSMC_MACHINE}
    COMMAND ${CMAKE_COMMAND} -E echo "[IAC build] DSMC native make target '${DSMC_MACHINE}' finished."
    COMMAND ${CMAKE_COMMAND} -E echo "[IAC build] Launcher available at ${CMAKE_BINARY_DIR}/bin/dsmc-iac."
    DEPENDS ${IAC_DSMC_TARGET_DEPENDS}
    COMMAND_EXPAND_LISTS
    COMMENT "Building private DSMC/IAC executable in the IAC build tree"
  )
else()
  add_custom_target(
    dsmc
    COMMAND ${CMAKE_COMMAND} -E echo "Cannot build DSMC/IAC overlay."
    COMMAND ${CMAKE_COMMAND} -E echo "Set DSMC_ROOT and ISTHMUS_ROOT, and make sure make is available."
    COMMAND ${CMAKE_COMMAND} -E false
  )
endif()

message(STATUS "ISTHMUS library: ${IAC_ISTHMUS_LIBRARY}")
message(STATUS "DSMC root: ${DSMC_ROOT}")
message(STATUS "DSMC machine: ${DSMC_MACHINE}")
if(IAC_ENABLE_MPI_TESTS AND IAC_MPIEXEC_EXECUTABLE)
  message(STATUS "DSMC/IAC MPI tests: enabled with ${IAC_MPIEXEC_EXECUTABLE} ${IAC_MPIEXEC_NUMPROC_FLAG} ${IAC_DSMC_MPI_NP}")
elseif(IAC_ENABLE_MPI_TESTS)
  message(WARNING "DSMC/IAC MPI tests: disabled because no MPI launcher was found")
else()
  message(STATUS "DSMC/IAC MPI tests: disabled by IAC_ENABLE_MPI_TESTS=OFF")
endif()
if(IAC_DSMC_USE_OVERLAY AND DSMC_ROOT)
  message(STATUS "DSMC/IAC target: cmake --build ${CMAKE_BINARY_DIR} --target dsmc")
  message(STATUS "DSMC/IAC executable: ${IAC_DSMC_EXECUTABLE}")
elseif(IAC_DSMC_EXECUTABLE)
  message(STATUS "DSMC/IAC executable: ${IAC_DSMC_EXECUTABLE}")
else()
  message(STATUS "DSMC/IAC overlay: disabled")
endif()

enable_testing()

set(IAC_TEST_DECLARED_COUNT 0)
set(IAC_TEST_RUNNABLE_COUNT 0)
set(IAC_TEST_SKIPPED_COUNT 0)
set(IAC_TEST_SUMMARY_LINES)

function(iac_test_requirement_status requirement available_var reason_var)
  string(TOUPPER "${requirement}" _requirement)
  set(_available TRUE)
  set(_reason "")

  if(_requirement STREQUAL "ISTHMUS")
    if(NOT isthmus_cpp_FOUND)
      set(_available FALSE)
      set(_reason "ISTHMUS library unavailable")
    endif()
  elseif(_requirement STREQUAL "PYTHON")
    if(NOT Python3_Interpreter_FOUND)
      set(_available FALSE)
      set(_reason "Python3 interpreter unavailable")
    endif()
  elseif(_requirement STREQUAL "PYTHON_FIXTURES")
    if(NOT TARGET generated-fixtures)
      set(_available FALSE)
      set(_reason "generated TIFF fixtures unavailable because Python3 was not found")
    endif()
  elseif(_requirement STREQUAL "DSMC")
    if(NOT IAC_DSMC_EXECUTABLE)
      set(_available FALSE)
      set(_reason "DSMC/IAC executable unavailable")
    elseif(NOT isthmus_cpp_FOUND)
      set(_available FALSE)
      set(_reason "DSMC/IAC tests require ISTHMUS")
    endif()
  elseif(_requirement STREQUAL "MPI")
    if(NOT IAC_ENABLE_MPI_TESTS)
      set(_available FALSE)
      set(_reason "MPI tests disabled by IAC_ENABLE_MPI_TESTS=OFF")
    elseif(NOT IAC_MPIEXEC_EXECUTABLE)
      set(_available FALSE)
      set(_reason "MPI launcher unavailable")
    endif()
  else()
    set(_available FALSE)
    set(_reason "unknown requirement '${requirement}'")
  endif()

  set(${available_var} "${_available}" PARENT_SCOPE)
  set(${reason_var} "${_reason}" PARENT_SCOPE)
endfunction()

function(iac_add_registered_test)
  set(options)
  set(one_value_args NAME)
  set(multi_value_args COMMAND LABELS PROPERTIES REQUIRES)
  cmake_parse_arguments(IAC_TEST "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

  if(NOT IAC_TEST_NAME)
    message(FATAL_ERROR "iac_add_registered_test requires NAME")
  endif()
  if(NOT IAC_TEST_COMMAND)
    message(FATAL_ERROR "iac_add_registered_test(${IAC_TEST_NAME}) requires COMMAND")
  endif()

  set(_missing_reasons)
  foreach(_requirement IN LISTS IAC_TEST_REQUIRES)
    iac_test_requirement_status("${_requirement}" _available _reason)
    if(NOT _available)
      list(APPEND _missing_reasons "${_reason}")
    endif()
  endforeach()

  string(REPLACE ";" "," _requires_text "${IAC_TEST_REQUIRES}")
  if(NOT _requires_text)
    set(_requires_text "-")
  endif()
  string(REPLACE ";" "," _labels_text "${IAC_TEST_LABELS}")
  if(NOT _labels_text)
    set(_labels_text "-")
  endif()

  math(EXPR _declared_count "${IAC_TEST_DECLARED_COUNT} + 1")
  set(IAC_TEST_DECLARED_COUNT "${_declared_count}" PARENT_SCOPE)

  if(_missing_reasons)
    string(REPLACE ";" ", " _missing_text "${_missing_reasons}")
    add_test(
      NAME ${IAC_TEST_NAME}
      COMMAND ${CMAKE_COMMAND}
              -DIAC_SKIP_REASON=${_missing_text}
              -P ${CMAKE_SOURCE_DIR}/tests/cmake/skip-test.cmake
    )
    set_tests_properties(${IAC_TEST_NAME} PROPERTIES
      SKIP_REGULAR_EXPRESSION "IAC test skipped:"
    )
    if(IAC_TEST_LABELS)
      set_tests_properties(${IAC_TEST_NAME} PROPERTIES
        LABELS "${IAC_TEST_LABELS}"
      )
    endif()
    math(EXPR _skipped_count "${IAC_TEST_SKIPPED_COUNT} + 1")
    set(IAC_TEST_SKIPPED_COUNT "${_skipped_count}" PARENT_SCOPE)
    set(_summary_line "SKIP\t${IAC_TEST_NAME}\trequires=${_requires_text}\tlabels=${_labels_text}\tmissing=${_missing_text}")
  else()
    add_test(NAME ${IAC_TEST_NAME} COMMAND ${IAC_TEST_COMMAND})
    set(_test_properties ${IAC_TEST_PROPERTIES})
    if(_test_properties)
      set_tests_properties(${IAC_TEST_NAME} PROPERTIES ${_test_properties})
    endif()
    if(IAC_TEST_LABELS)
      set_tests_properties(${IAC_TEST_NAME} PROPERTIES
        LABELS "${IAC_TEST_LABELS}"
      )
    endif()
    math(EXPR _runnable_count "${IAC_TEST_RUNNABLE_COUNT} + 1")
    set(IAC_TEST_RUNNABLE_COUNT "${_runnable_count}" PARENT_SCOPE)
    set(_summary_line "RUN\t${IAC_TEST_NAME}\trequires=${_requires_text}\tlabels=${_labels_text}")
  endif()

  set(_summary_lines "${IAC_TEST_SUMMARY_LINES}")
  list(APPEND _summary_lines "${_summary_line}")
  set(IAC_TEST_SUMMARY_LINES "${_summary_lines}" PARENT_SCOPE)
endfunction()

macro(iac_add_hosted_input_test test_name input_file)
  set(_iac_hosted_labels hosted dsmc ${ARGN})
  set(_iac_hosted_mpi_labels hosted dsmc mpi ${ARGN})
  iac_add_registered_test(
    NAME hosted-${test_name}
    REQUIRES DSMC
    LABELS ${_iac_hosted_labels}
    COMMAND ${IAC_DSMC_EXECUTABLE} -log none
            -in ${CMAKE_SOURCE_DIR}/${input_file}
  )

  iac_add_registered_test(
    NAME hosted-mpi-${test_name}
    REQUIRES DSMC MPI
    LABELS ${_iac_hosted_mpi_labels}
    COMMAND ${IAC_MPIEXEC_EXECUTABLE} ${IAC_MPIEXEC_NUMPROC_FLAG} ${IAC_DSMC_MPI_NP}
            ${IAC_DSMC_EXECUTABLE} -log none
            -in ${CMAKE_SOURCE_DIR}/${input_file}
  )
endmacro()

macro(iac_add_dsmc_input_test test_name input_file)
  iac_add_registered_test(
    NAME ${test_name}
    REQUIRES DSMC
    LABELS dsmc
    COMMAND ${IAC_DSMC_EXECUTABLE} -log none
            -in ${CMAKE_SOURCE_DIR}/${input_file}
  )

  string(REGEX REPLACE "^dsmc-" "" mpi_test_suffix "${test_name}")
  iac_add_registered_test(
    NAME dsmc-mpi-${mpi_test_suffix}
    REQUIRES DSMC MPI
    LABELS dsmc mpi
    COMMAND ${IAC_MPIEXEC_EXECUTABLE} ${IAC_MPIEXEC_NUMPROC_FLAG} ${IAC_DSMC_MPI_NP}
            ${IAC_DSMC_EXECUTABLE} -log none
            -in ${CMAKE_SOURCE_DIR}/${input_file}
  )
endmacro()

macro(iac_add_dsmc_input_test_serial test_name input_file)
  iac_add_registered_test(
    NAME ${test_name}
    REQUIRES DSMC
    LABELS dsmc
    COMMAND ${IAC_DSMC_EXECUTABLE} -log none
            -in ${CMAKE_SOURCE_DIR}/${input_file}
  )
endmacro()

set(IAC_TIFF_DEFAULT_AXES_TEST_INPUT
    "${CMAKE_BINARY_DIR}/tests/inputs/tiff-axis/in.tiff-default-axes.verify")
set(IAC_TIFF_LEGACY_AXES_TEST_INPUT
    "${CMAKE_BINARY_DIR}/tests/inputs/tiff-axis/in.tiff-legacy-axes.verify")
set(IAC_TIFF_CENTER_ORIGIN_TEST_INPUT
    "${CMAKE_BINARY_DIR}/tests/inputs/tiff-axis/in.tiff-center-origin.verify")
set(IAC_CARBON_CONSTANT_TEST_INPUT
    "${CMAKE_BINARY_DIR}/tests/inputs/pregen-tiff-carbon-recession/in.constant-flux.verify")
set(IAC_MULTIPHASE_SLAB_TIFF_TEST_INPUT
    "${CMAKE_BINARY_DIR}/tests/inputs/multiphase-side-by-side-slabs/in.multiphase-side-by-side-slabs-tiff.verify")
set(IAC_DSMC_TIFF_CHEMISTRY_TEST_INPUT
    "${CMAKE_BINARY_DIR}/tests/inputs/pregen-tiff-carbon-recession/in.dsmc-co.verify")

set(SOURCE_DIR ${CMAKE_SOURCE_DIR})
set(BINARY_DIR ${CMAKE_BINARY_DIR})

if(TARGET generated-fixtures)
  configure_file(
    ${CMAKE_SOURCE_DIR}/tests/inputs/tiff-axis/in.tiff-default-axes.verify.in
    ${IAC_TIFF_DEFAULT_AXES_TEST_INPUT}
    @ONLY
  )
  configure_file(
    ${CMAKE_SOURCE_DIR}/tests/inputs/tiff-axis/in.tiff-legacy-axes.verify.in
    ${IAC_TIFF_LEGACY_AXES_TEST_INPUT}
    @ONLY
  )
  configure_file(
    ${CMAKE_SOURCE_DIR}/tests/inputs/tiff-axis/in.tiff-center-origin.verify.in
    ${IAC_TIFF_CENTER_ORIGIN_TEST_INPUT}
    @ONLY
  )
  configure_file(
    ${CMAKE_SOURCE_DIR}/tests/inputs/pregen-tiff-carbon-recession/in.constant-flux.verify.in
    ${IAC_CARBON_CONSTANT_TEST_INPUT}
    @ONLY
  )
  configure_file(
    ${CMAKE_SOURCE_DIR}/tests/inputs/multiphase-side-by-side-slabs/in.multiphase-side-by-side-slabs-tiff.verify.in
    ${IAC_MULTIPHASE_SLAB_TIFF_TEST_INPUT}
    @ONLY
  )
  configure_file(
    ${CMAKE_SOURCE_DIR}/tests/inputs/pregen-tiff-carbon-recession/in.dsmc-co.verify.in
    ${IAC_DSMC_TIFF_CHEMISTRY_TEST_INPUT}
    @ONLY
  )
endif()

iac_add_registered_test(
  NAME generated-fixtures-build
  REQUIRES PYTHON_FIXTURES
  LABELS fixtures python
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target generated-fixtures
)

iac_add_registered_test(
  NAME slab-direct-command-verification
  LABELS standalone
  COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/slab-direct-ablation/in.slab-direct-command.verify
)

iac_add_registered_test(
  NAME slab-direct-yhi-verification
  LABELS standalone
  COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/slab-direct-ablation/in.slab-direct-yhi.verify
)

iac_add_registered_test(
  NAME multiphase-side-by-side-slabs-verification
  LABELS standalone multiphase slab
  COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/multiphase-side-by-side-slabs/in.multiphase-side-by-side-slabs.verify
)

iac_add_registered_test(
  NAME multiphase-core-shell-sphere-verification
  REQUIRES ISTHMUS
  LABELS standalone isthmus multiphase sphere
  COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/multiphase-core-shell-sphere/in.multiphase-core-shell-sphere.verify
)

iac_add_registered_test(
  NAME slab-isthmus-finite-surface-verification
  REQUIRES ISTHMUS
  LABELS standalone isthmus
  COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/slab-isthmus-ablation/in.slab-isthmus-finite-surface.verify
)
iac_add_registered_test(
  NAME slab-isthmus-ghost-wall-verification
  REQUIRES ISTHMUS
  LABELS standalone isthmus
  COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/slab-isthmus-ablation/in.slab-isthmus-ghost-wall.verify
)
iac_add_registered_test(
  NAME sphere-isthmus-local-deletion-verification
  REQUIRES ISTHMUS
  LABELS standalone isthmus
  COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-local-deletion.verify
)
iac_add_registered_test(
  NAME sphere-isthmus-normal-carryover-verification
  REQUIRES ISTHMUS
  LABELS standalone isthmus
  COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-normal-carryover.verify
)
iac_add_registered_test(
  NAME sphere-isthmus-kinetic-theory-verification
  REQUIRES ISTHMUS
  LABELS standalone isthmus
  COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-kinetic-theory.verify
)
iac_add_registered_test(
  NAME sphere-isthmus-normal-carryover-convergence
  REQUIRES ISTHMUS
  LABELS standalone isthmus convergence
  COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-normal-carryover-convergence.verify
)
iac_add_registered_test(
  NAME sphere-isthmus-surface-resolution-convergence
  REQUIRES ISTHMUS
  LABELS standalone isthmus convergence
  COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-surface-resolution-convergence.verify
)
iac_add_registered_test(
  NAME tiff-sphere-normal-carryover-verification
  REQUIRES ISTHMUS
  LABELS standalone isthmus tiff
  COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/tiff-sphere/in.tiff-sphere-normal-carryover.verify
)

iac_add_registered_test(
  NAME tiff-default-axes-verification
  REQUIRES PYTHON_FIXTURES
  LABELS standalone tiff python
  PROPERTIES DEPENDS generated-fixtures-build
  COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}
          $<TARGET_FILE:ia-core> -in ${IAC_TIFF_DEFAULT_AXES_TEST_INPUT}
)
iac_add_registered_test(
  NAME tiff-legacy-axes-verification
  REQUIRES PYTHON_FIXTURES
  LABELS standalone tiff python
  PROPERTIES DEPENDS generated-fixtures-build
  COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}
          $<TARGET_FILE:ia-core> -in ${IAC_TIFF_LEGACY_AXES_TEST_INPUT}
)
iac_add_registered_test(
  NAME tiff-center-origin-verification
  REQUIRES PYTHON_FIXTURES
  LABELS standalone tiff python
  PROPERTIES DEPENDS generated-fixtures-build
  COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}
          $<TARGET_FILE:ia-core> -in ${IAC_TIFF_CENTER_ORIGIN_TEST_INPUT}
)
iac_add_registered_test(
  NAME pregen-tiff-carbon-recession-constant-flux-verification
  REQUIRES PYTHON_FIXTURES
  LABELS standalone tiff python
  PROPERTIES DEPENDS generated-fixtures-build
  COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}
          $<TARGET_FILE:ia-core> -in ${IAC_CARBON_CONSTANT_TEST_INPUT}
)
iac_add_registered_test(
  NAME multiphase-side-by-side-slabs-tiff-verification
  REQUIRES PYTHON_FIXTURES
  LABELS standalone multiphase slab tiff python
  PROPERTIES DEPENDS generated-fixtures-build
  COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}
          $<TARGET_FILE:ia-core> -in ${IAC_MULTIPHASE_SLAB_TIFF_TEST_INPUT}
)

iac_add_hosted_input_test(
  slab-direct-command-verification
  tests/inputs/slab-direct-ablation/in.slab-direct-command.verify
)
iac_add_hosted_input_test(
  slab-direct-yhi-verification
  tests/inputs/slab-direct-ablation/in.slab-direct-yhi.verify
)
iac_add_hosted_input_test(
  multiphase-side-by-side-slabs-verification
  tests/inputs/multiphase-side-by-side-slabs/in.multiphase-side-by-side-slabs.verify
  multiphase slab
)
iac_add_hosted_input_test(
  multiphase-core-shell-sphere-verification
  tests/inputs/multiphase-core-shell-sphere/in.multiphase-core-shell-sphere.verify
  multiphase sphere
)
iac_add_hosted_input_test(
  slab-isthmus-finite-surface-verification
  tests/inputs/slab-isthmus-ablation/in.slab-isthmus-finite-surface.verify
)
iac_add_hosted_input_test(
  slab-isthmus-ghost-wall-verification
  tests/inputs/slab-isthmus-ablation/in.slab-isthmus-ghost-wall.verify
)
iac_add_hosted_input_test(
  sphere-isthmus-local-deletion-verification
  tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-local-deletion.verify
)
iac_add_hosted_input_test(
  sphere-isthmus-normal-carryover-verification
  tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-normal-carryover.verify
)
iac_add_hosted_input_test(
  sphere-isthmus-kinetic-theory-verification
  tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-kinetic-theory.verify
)
iac_add_hosted_input_test(
  tiff-sphere-normal-carryover-verification
  tests/inputs/tiff-sphere/in.tiff-sphere-normal-carryover.verify
)

iac_add_dsmc_input_test(
  dsmc-slab-time-stop-verification
  tests/inputs/dsmc-slab-time-stop/in.dsmc-slab-time-stop.verify
)
iac_add_dsmc_input_test(
  dsmc-sphere-flux-verification
  tests/inputs/dsmc-sphere-flux/in.dsmc-sphere-flux.verify
)

iac_add_dsmc_input_test_serial(
  dsmc-mass-flux-explicit-verification
  tests/inputs/dsmc-sphere-mass-flux/in.dsmc-mass-flux-explicit.verify
)
iac_add_dsmc_input_test_serial(
  dsmc-converge-verification
  tests/inputs/dsmc-sphere-mass-flux/in.dsmc-converge.verify
)
iac_add_registered_test(
  NAME dsmc-converge-zero-flux-guard-verification
  REQUIRES DSMC
  LABELS dsmc
  PROPERTIES
    WILL_FAIL TRUE
    PASS_REGULAR_EXPRESSION "dsmc_converge flux did not converge before max-iter"
  COMMAND ${IAC_DSMC_EXECUTABLE} -log none
          -in ${CMAKE_SOURCE_DIR}/tests/inputs/dsmc-sphere-mass-flux/in.dsmc-converge-zero-flux.verify
)
iac_add_dsmc_input_test_serial(
  dsmc-mass-flux-ablation-verification
  tests/inputs/dsmc-sphere-mass-flux/in.dsmc-mass-flux-ablation.verify
)
iac_add_dsmc_input_test(
  dsmc-converge-ablation-loop-verification
  tests/inputs/dsmc-sphere-mass-flux/in.dsmc-converge-ablation-loop.verify
)
set_tests_properties(
  dsmc-mass-flux-explicit-verification
  dsmc-converge-verification
  dsmc-mass-flux-ablation-verification
  PROPERTIES ENVIRONMENT "OMPI_MCA_btl=self"
)

iac_add_registered_test(
  NAME dsmc-sphere-kinetic-convergence
  REQUIRES DSMC
  LABELS dsmc convergence
  PROPERTIES TIMEOUT 90
  COMMAND ${CMAKE_COMMAND}
          -DDSMC_EXECUTABLE=${IAC_DSMC_EXECUTABLE}
          -DCHECKER=$<TARGET_FILE:dsmc-kinetic-convergence-check>
          -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
          -DBINARY_DIR=${CMAKE_BINARY_DIR}
          -P ${CMAKE_SOURCE_DIR}/tests/cmake/run-dsmc-sphere-kinetic-convergence.cmake
)

iac_add_registered_test(
  NAME dsmc-mpi-isthmus-restart-verification
  REQUIRES DSMC MPI
  LABELS dsmc mpi isthmus restart
  PROPERTIES TIMEOUT 60
  COMMAND ${CMAKE_COMMAND}
          -DDSMC_EXECUTABLE=${IAC_DSMC_EXECUTABLE}
          -DMPIEXEC_EXECUTABLE=${IAC_MPIEXEC_EXECUTABLE}
          -DMPIEXEC_NUMPROC_FLAG=${IAC_MPIEXEC_NUMPROC_FLAG}
          -DMPI_NP=${IAC_DSMC_MPI_NP}
          -DBINARY_DIR=${CMAKE_BINARY_DIR}
          -P ${CMAKE_SOURCE_DIR}/tests/cmake/run-dsmc-mpi-isthmus-restart.cmake
)

iac_add_registered_test(
  NAME hosted-multiphase-side-by-side-slabs-tiff-verification
  REQUIRES DSMC PYTHON_FIXTURES
  LABELS hosted dsmc multiphase slab tiff python
  PROPERTIES DEPENDS generated-fixtures-build
  COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}
          ${IAC_DSMC_EXECUTABLE} -log none
          -in ${IAC_MULTIPHASE_SLAB_TIFF_TEST_INPUT}
)
iac_add_registered_test(
  NAME hosted-mpi-multiphase-side-by-side-slabs-tiff-verification
  REQUIRES DSMC MPI PYTHON_FIXTURES
  LABELS hosted dsmc mpi multiphase slab tiff python
  PROPERTIES DEPENDS generated-fixtures-build
  COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}
          ${IAC_MPIEXEC_EXECUTABLE} ${IAC_MPIEXEC_NUMPROC_FLAG} ${IAC_DSMC_MPI_NP}
          ${IAC_DSMC_EXECUTABLE} -log none
          -in ${IAC_MULTIPHASE_SLAB_TIFF_TEST_INPUT}
)
iac_add_registered_test(
  NAME pregen-tiff-carbon-recession-dsmc-co-verification
  REQUIRES DSMC PYTHON_FIXTURES
  LABELS dsmc tiff python
  PROPERTIES DEPENDS generated-fixtures-build TIMEOUT 90
  COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}
          ${IAC_DSMC_EXECUTABLE} -log none
          -in ${IAC_DSMC_TIFF_CHEMISTRY_TEST_INPUT}
)
iac_add_registered_test(
  NAME pregen-tiff-carbon-recession-dsmc-co-converge-verification
  REQUIRES DSMC PYTHON_FIXTURES
  LABELS dsmc tiff python convergence
  PROPERTIES DEPENDS generated-fixtures-build TIMEOUT 30 ENVIRONMENT "OMPI_MCA_btl=self"
  COMMAND ${CMAKE_COMMAND}
          -DDSMC_EXECUTABLE=${IAC_DSMC_EXECUTABLE}
          -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
          -DBINARY_DIR=${CMAKE_BINARY_DIR}
          -DCARBON_TIFF=${IAC_CARBON_SAMPLE_TIFF}
          -P ${CMAKE_SOURCE_DIR}/tests/cmake/run-pregen-tiff-carbon-converge.cmake
)

set(IAC_TEST_SUMMARY_FILE "${CMAKE_BINARY_DIR}/test-summary.txt")
file(WRITE "${IAC_TEST_SUMMARY_FILE}"
     "IAC test plan\n"
     "=============\n\n"
     "Declared tests: ${IAC_TEST_DECLARED_COUNT}\n"
     "Runnable tests: ${IAC_TEST_RUNNABLE_COUNT}\n"
     "Skipped tests: ${IAC_TEST_SKIPPED_COUNT}\n\n"
     "Capabilities:\n")
if(isthmus_cpp_FOUND)
  file(APPEND "${IAC_TEST_SUMMARY_FILE}" "  ISTHMUS: enabled (${IAC_ISTHMUS_LIBRARY})\n")
else()
  file(APPEND "${IAC_TEST_SUMMARY_FILE}" "  ISTHMUS: disabled\n")
endif()
if(Python3_Interpreter_FOUND)
  file(APPEND "${IAC_TEST_SUMMARY_FILE}" "  Python fixtures: enabled (${Python3_EXECUTABLE})\n")
else()
  file(APPEND "${IAC_TEST_SUMMARY_FILE}" "  Python fixtures: disabled (Python3 not found)\n")
endif()
if(IAC_DSMC_EXECUTABLE)
  file(APPEND "${IAC_TEST_SUMMARY_FILE}" "  DSMC executable: enabled (${IAC_DSMC_EXECUTABLE})\n")
else()
  file(APPEND "${IAC_TEST_SUMMARY_FILE}" "  DSMC executable: disabled\n")
endif()
if(IAC_ENABLE_MPI_TESTS AND IAC_MPIEXEC_EXECUTABLE)
  file(APPEND "${IAC_TEST_SUMMARY_FILE}" "  MPI tests: enabled (${IAC_MPIEXEC_EXECUTABLE} ${IAC_MPIEXEC_NUMPROC_FLAG} ${IAC_DSMC_MPI_NP})\n")
elseif(IAC_ENABLE_MPI_TESTS)
  file(APPEND "${IAC_TEST_SUMMARY_FILE}" "  MPI tests: disabled (MPI launcher not found)\n")
else()
  file(APPEND "${IAC_TEST_SUMMARY_FILE}" "  MPI tests: disabled (IAC_ENABLE_MPI_TESTS=OFF)\n")
endif()

file(APPEND "${IAC_TEST_SUMMARY_FILE}" "\nTests:\n")
foreach(_summary_line IN LISTS IAC_TEST_SUMMARY_LINES)
  file(APPEND "${IAC_TEST_SUMMARY_FILE}" "${_summary_line}\n")
endforeach()

message(STATUS "IAC test plan: ${IAC_TEST_DECLARED_COUNT} declared, ${IAC_TEST_RUNNABLE_COUNT} runnable, ${IAC_TEST_SKIPPED_COUNT} skipped")
message(STATUS "IAC test summary: ${IAC_TEST_SUMMARY_FILE}")

get_property(IAC_REGISTERED_TESTS DIRECTORY PROPERTY TESTS)
if(IAC_REGISTERED_TESTS)
  set_tests_properties(${IAC_REGISTERED_TESTS} PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
endif()

add_custom_target(
  check-verbose
  COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} --output-on-failure --verbose
  DEPENDS ia-core
  COMMENT "Running tests with console stats output"
)

if(Python3_Interpreter_FOUND AND PDFLATEX_EXECUTABLE)
  add_custom_target(
    docs-pdf
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/build-docs-pdf.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building docs/isthmus-ablation-core-manual.pdf"
  )

  add_custom_target(
    test-report
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/run-test-report.py
            all
            --build ${CMAKE_BINARY_DIR}
            --manifest ${CMAKE_SOURCE_DIR}/tests/report-cases.csv
            --out ${CMAKE_BINARY_DIR}/output/test-report.tex
    DEPENDS ia-core
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building combined verification report"
  )

endif()
