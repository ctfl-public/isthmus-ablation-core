cmake_minimum_required(VERSION 3.16)

project(isthmus_ablation_core LANGUAGES CXX)

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

find_package(isthmus_cpp CONFIG QUIET PATHS
  "$ENV{ISTHMUS_CPP_DIR}"
  "/Users/tstoffel1/isthmus/build-wsl"
)

if(isthmus_cpp_FOUND)
  target_link_libraries(isthmus_ablation_core PUBLIC isthmus_cpp::isthmus_cpp)
  target_compile_definitions(isthmus_ablation_core PRIVATE IAC_HAS_ISTHMUS=1)
endif()

if(MSVC)
  target_compile_options(isthmus_ablation_core PRIVATE /W4)
else()
  target_compile_options(isthmus_ablation_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(ia-core src/main.cpp)
target_link_libraries(ia-core PRIVATE isthmus_ablation_core)

find_package(Python3 COMPONENTS Interpreter)
find_program(PDFLATEX_EXECUTABLE pdflatex)
set(IAC_DSMC_EXECUTABLE "" CACHE FILEPATH "Optional DSMC/SPARTA executable for coupled tests")

enable_testing()

add_test(
  NAME slab-direct-command-verification
  COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/slab-direct-ablation/in.slab-direct-command.verify
)

if(isthmus_cpp_FOUND)
  add_test(
    NAME slab-isthmus-finite-surface-verification
    COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/slab-isthmus-ablation/in.slab-isthmus-finite-surface.verify
  )
  add_test(
    NAME slab-isthmus-ghost-wall-verification
    COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/slab-isthmus-ablation/in.slab-isthmus-ghost-wall.verify
  )
  add_test(
    NAME sphere-isthmus-local-deletion-verification
    COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-local-deletion.verify
  )
  add_test(
    NAME sphere-isthmus-normal-carryover-verification
    COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-normal-carryover.verify
  )
  add_test(
    NAME sphere-isthmus-kinetic-theory-verification
    COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-kinetic-theory.verify
  )
  add_test(
    NAME sphere-isthmus-normal-carryover-convergence
    COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-normal-carryover-convergence.verify
  )
endif()

if(IAC_DSMC_EXECUTABLE AND isthmus_cpp_FOUND)
  add_test(
    NAME dsmc-slab-direct-ablation-verification
    COMMAND ${IAC_DSMC_EXECUTABLE} -screen none -log none
            -in ${CMAKE_SOURCE_DIR}/tests/inputs/dsmc-slab-direct-ablation/in.dsmc-slab-direct-ablation.verify
  )
  set_tests_properties(dsmc-slab-direct-ablation-verification PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )

  add_test(
    NAME dsmc-sphere-isthmus-normal-carryover-verification
    COMMAND ${IAC_DSMC_EXECUTABLE} -screen none -log none
            -in ${CMAKE_SOURCE_DIR}/tests/inputs/dsmc-sphere-isthmus-ablation/in.dsmc-sphere-isthmus-normal-carryover.verify
  )
  set_tests_properties(dsmc-sphere-isthmus-normal-carryover-verification PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )

  add_test(
    NAME dsmc-sphere-flux-verification
    COMMAND ${IAC_DSMC_EXECUTABLE} -screen none -log none
            -in ${CMAKE_SOURCE_DIR}/tests/inputs/dsmc-sphere-flux/in.dsmc-sphere-flux.verify
  )
  set_tests_properties(dsmc-sphere-flux-verification PROPERTIES
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

  if(IAC_DSMC_EXECUTABLE)
    add_custom_target(
      dsmc-convergence-report
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/run-dsmc-kinetic-convergence.py
              --dsmc ${IAC_DSMC_EXECUTABLE}
              --out ${CMAKE_BINARY_DIR}/output/dsmc-sphere-kinetic-convergence
              --steps 5,20,80
              --loops 6
              --resolution 10
              --ablation-update-time 0.001
              --fnum 2.0e12
              --tolerance-percent 5.0
              --pdf
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMENT "Building DSMC sphere kinetic convergence report"
    )

    add_custom_target(
      dsmc-half-mass-report
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/run-dsmc-kinetic-convergence.py
              --dsmc ${IAC_DSMC_EXECUTABLE}
              --out ${CMAKE_BINARY_DIR}/output/dsmc-sphere-kinetic-half-mass
              --steps 5,20,80
              --loops 8
              --resolution 5
              --ablation-update-time 0.0125
              --fnum 5.0e12
              --tolerance-percent 110.0
              --pdf
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMENT "Building DSMC sphere half-mass convergence report"
    )

    add_custom_target(
      dsmc-visual-case
      COMMAND ${IAC_DSMC_EXECUTABLE} -screen none -log output/log.sparta
              -in in.dsmc-sphere-visual
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/convert-dsmc-visual.py
              ${CMAKE_SOURCE_DIR}/examples/dsmc-sphere-visual
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/examples/dsmc-sphere-visual
      COMMENT "Running DSMC sphere visualization case and converting outputs to VTK"
    )
  endif()
endif()
