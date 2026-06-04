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

enable_testing()

add_test(
  NAME slab-local-verification
  COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/slab-local-ablation/in.slab-local-ablation.verify
)

add_test(
  NAME slab-local-fix-verification
  COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/slab-local-ablation/in.slab-local-ablation.fix-verify
)

if(isthmus_cpp_FOUND)
  add_test(
    NAME sphere-isthmus-local-verification
    COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-local.verify
  )
  add_test(
    NAME sphere-isthmus-normal-verification
    COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-normal.verify
  )
  add_test(
    NAME sphere-isthmus-normal-convergence
    COMMAND $<TARGET_FILE:ia-core> -in ${CMAKE_SOURCE_DIR}/tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-normal-convergence.verify
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
