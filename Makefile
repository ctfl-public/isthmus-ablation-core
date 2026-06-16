# SPARTA-style build front end for isthmus-ablation-core.
#
# CMake still owns dependency discovery and build generation. This file keeps
# day-to-day commands compact on clusters and local machines.

CMAKE ?= cmake
CTEST ?= ctest

BUILD_TYPE ?= Debug
BUILD_DIR ?= build
DSMC_BUILD_DIR ?= build-dsmc

DSMC_ROOT ?= $(HOME)/dsmc
ISTHMUS_ROOT ?= $(HOME)/isthmus
DSMC_MACHINE ?= mpi

IAC_DSMC_MPI_NP ?= 2
IAC_ENABLE_MPI_TESTS ?= ON
CMAKE_ARGS ?=
CTEST_ARGS ?=

CMAKE_COMMON_ARGS = \
	-G "Unix Makefiles" \
	-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	-DISTHMUS_ROOT=$(ISTHMUS_ROOT)

.DEFAULT_GOAL := help

.PHONY: help
help:
	@echo "isthmus-ablation-core build targets"
	@echo
	@echo "  make standalone       Build standalone build/ia-core"
	@echo "  make mpi              Build DSMC/IAC with DSMC machine target mpi"
	@echo "  make mac_mpi          Build DSMC/IAC with DSMC machine target mac_mpi"
	@echo "  make serial           Build DSMC/IAC with DSMC machine target serial"
	@echo "  make dsmc DSMC_MACHINE=<machine>"
	@echo "                        Build DSMC/IAC with an explicit DSMC machine target"
	@echo
	@echo "  make test-standalone  Run standalone CTest suite"
	@echo "  make test-dsmc        Run DSMC/IAC CTest suite"
	@echo "  make test-dsmc-serial Run DSMC/IAC CTest suite without MPI-launched tests"
	@echo "  make test             Alias for test-dsmc"
	@echo
	@echo "  make check-standalone Build standalone and run standalone tests"
	@echo "  make check-mpi        Build mpi and run DSMC/IAC tests"
	@echo "  make check-mac_mpi    Build mac_mpi and run DSMC/IAC tests"
	@echo
	@echo "  make docs             Build docs/isthmus-ablation-core-manual.pdf"
	@echo "  make report           Build verification report"
	@echo "  make clean            Remove build directories"
	@echo
	@echo "Variables:"
	@echo "  DSMC_ROOT=$(DSMC_ROOT)"
	@echo "  ISTHMUS_ROOT=$(ISTHMUS_ROOT)"
	@echo "  BUILD_TYPE=$(BUILD_TYPE)"

.PHONY: configure-standalone standalone
configure-standalone:
	$(CMAKE) -S . -B $(BUILD_DIR) \
	  $(CMAKE_COMMON_ARGS) \
	  -DIAC_DSMC_USE_OVERLAY=OFF \
	  -DIAC_DSMC_EXECUTABLE="" \
	  $(CMAKE_ARGS)

standalone: configure-standalone
	$(CMAKE) --build $(BUILD_DIR)

.PHONY: configure-dsmc dsmc
configure-dsmc:
	$(CMAKE) -S . -B $(DSMC_BUILD_DIR) \
	  $(CMAKE_COMMON_ARGS) \
	  -DIAC_DSMC_USE_OVERLAY=ON \
	  -DDSMC_ROOT=$(DSMC_ROOT) \
	  -DDSMC_MACHINE=$(DSMC_MACHINE) \
	  -DIAC_DSMC_MPI_NP=$(IAC_DSMC_MPI_NP) \
	  -DIAC_ENABLE_MPI_TESTS=$(IAC_ENABLE_MPI_TESTS) \
	  $(CMAKE_ARGS)

dsmc: configure-dsmc
	$(CMAKE) --build $(DSMC_BUILD_DIR) --target dsmc

.PHONY: mpi mac_mpi serial mcc
mpi:
	$(MAKE) dsmc DSMC_MACHINE=mpi

mac_mpi:
	$(MAKE) dsmc DSMC_MACHINE=mac_mpi

serial:
	$(MAKE) dsmc DSMC_MACHINE=serial

mcc:
	$(MAKE) dsmc DSMC_MACHINE=mpi

.PHONY: test-standalone test-dsmc test-dsmc-serial test
test-standalone:
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure $(CTEST_ARGS)

test-dsmc:
	$(CTEST) --test-dir $(DSMC_BUILD_DIR) --output-on-failure $(CTEST_ARGS)

test-dsmc-serial:
	$(CTEST) --test-dir $(DSMC_BUILD_DIR) --output-on-failure -E '(^hosted-mpi-|^dsmc-mpi-)' $(CTEST_ARGS)

test: test-dsmc

.PHONY: check-standalone check-mpi check-mac_mpi check-serial check-mcc
check-standalone: standalone test-standalone

check-mpi: mpi test-dsmc

check-mac_mpi: mac_mpi test-dsmc

check-serial: serial test-dsmc

check-mcc: mcc test-dsmc

.PHONY: docs report
docs: configure-standalone
	$(CMAKE) --build $(BUILD_DIR) --target docs-pdf

report: configure-standalone
	$(CMAKE) --build $(BUILD_DIR) --target test-report

.PHONY: clean clean-standalone clean-dsmc
clean-standalone:
	rm -rf $(BUILD_DIR)

clean-dsmc:
	rm -rf $(DSMC_BUILD_DIR)

clean: clean-standalone clean-dsmc
