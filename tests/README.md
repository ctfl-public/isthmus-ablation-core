# Tests

The top-level Makefile runs the useful test matrix:

```bash
make mpi
make test-dsmc
```

Inspect registered tests without running them:

```bash
cmake -E chdir build-dsmc ctest -N
```

Each configure also writes a test plan summary:

```bash
cat build-dsmc/test-summary.txt
```

The summary lists declared, runnable, and skipped tests, then gives each test's
requirements and missing capability reasons. Use these Make targets to generate
the summary and ask CTest for the registered list:

```bash
make test-plan-standalone
make test-plan-dsmc
```

## Test Groups

| Group | Names | What they prove |
| --- | --- | --- |
| Standalone IAC | `slab-*`, `sphere-*`, `tiff-*` | The core library, standalone parser, ISTHMUS path, ablation policies, dumps, and exact-solution checks work without DSMC. |
| DSMC-hosted parity | `hosted-*` | The same pure-IAC input files can run through the DSMC executable. |
| DSMC-hosted MPI parity | `hosted-mpi-*` | The same bridge commands do not break when DSMC launches with MPI. |
| DSMC gas-domain checks | `dsmc-*` and `dsmc-mpi-*` | Actual DSMC setup, surface installation, surface flux measurement, bridge verification, and coupled particle-driven recession work. |

`dsmc-mpi-isthmus-restart-verification` writes a DSMC/IAC restart after
`isthmus_surface` and `surf_install`, then reads it on the same MPI rank count
and with `read_restart ... balance rcb cell`. It guards the restart-time
surface-to-grid remapping path.

## Input Organization

Examples live under `examples/` and should be useful for visual inspection. They
may write VTU, VTP, and history output.

Tests live under `tests/inputs/`. When possible, a test input includes an
example, disables expensive visual dumps, and adds `iac_verify` checks. This
keeps examples and tests from becoming two separate simulations.

Use these comments at the top of new inputs:

```text
# Shared standalone/DSMC-hosted verification input.
# This file should run with ia-core and build-dsmc/bin/dsmc-iac.
```

or:

```text
# DSMC gas-domain verification input.
# This file requires build-dsmc/bin/dsmc-iac.
```

## Pass/Fail Criteria

Most checks use percent error because the quantities span different scales.
`norm max` checks an entire history, `norm final` checks the last row, and
convergence tests compare observed order across variable overrides.

Long exploratory convergence scripts and plotting reports are intentionally not
part of the default test suite. Keep those untracked or under `artifacts/` until
they become fast, deterministic regression tests.

## Coupled DSMC Kinetic Recession

`dsmc-sphere-kinetic-convergence` is the compact coupled DSMC recession test. It
does not use a Python runner. CTest configures three generated DSMC inputs at 4,
6, and 8 voxels across a 1 mm carbon sphere, runs the private DSMC/IAC
executable, and then calls the compiled
`dsmc-kinetic-convergence-check` helper.

The DSMC runs sample pure O2 incident number flux on the ISTHMUS triangles with
native `compute surf` and `fix ave/surf`. IAC converts that particle-sampled
flux into per-triangle carbon mass flux, maps it back to voxels, applies
normal-directed carryover, deletes depleted voxels, regenerates the ISTHMUS
surface, and repeats to a fixed physical ablation time.

The checker compares final `mf`, `vf`, and `rad` against the
continuum sphere solution:

```text
Gamma = n*sqrt(kB*T/(2*pi*m))
j = Gamma*Msolid/NA
R(t) = R0 - j*t/rho
m/m0 = (R/R0)^3
```

The pass/fail criterion is intentionally regression-test sized: the finest grid
must satisfy the configured percent-error tolerances, and the mf
error must improve from the coarsest grid to the finest grid. The output summary
is written to:

```text
build-dsmc/output/dsmc-sphere-kinetic-convergence/summary.csv
```

## Rough Carbon TIFF Tests

`pregen-tiff-carbon-recession-constant-flux-verification` uses the generated
rough carbon TIFF fixture, one-sided/both-sided ghost boundaries, ISTHMUS
top-surface selection, and normal carryover. It checks that a prescribed
constant top-surface mass flux recesses the sample to roughly the expected final
mass and volume fractions.

`pregen-tiff-carbon-recession-dsmc-co-verification` uses the same generated
carbon sample, but drives recession from DSMC/SPARTA surface chemistry. It
installs the ISTHMUS surface into DSMC, reacts O2 into CO at the surface, reads
`compute react/surf` through `fix ave/surf`, maps reaction-derived carbon loss
back to voxels, and checks the final mass and volume fractions. The case is
intentionally small and uses no gas-gas collision model so it remains a fast
workflow regression.

`pregen-tiff-carbon-recession-dsmc-co-converge-verification` uses the same
rough carbon TIFF fixture and surface chemistry, but reads DSMC's direct
`compute react/surf/mass/flux` output. It runs four compact
chemistry/ablation/remesh cycles, using `dsmc_converge` before each voxel
update, and verifies the final `mf` and `vf`. The standalone simulation input
lives in `examples/pregen-tiff-carbon-recession` with visual dumps enabled. The
CTest wrapper stages that example, the local species/reaction files, and the
generated TIFF into a private run directory; the test input includes the
example and adds pass/fail criteria.

## Direct DSMC Mass-Flux Tests

When the configured DSMC source tree contains `compute_react_surf_mass_flux`,
CTest also registers `dsmc-mass-flux-*` checks. These exercise the
`React_to_Flux` workflow where DSMC computes per-triangle solid mass flux
directly from surface reactions and IAC only reads, verifies, maps, and ablates
that flux.

`dsmc-mass-flux-explicit-verification` runs one DSMC step on a fixed sphere
and compares area-averaged `kg/m2/s` against ideal-gas impingement theory.

`dsmc-converge-verification` covers the compact `dsmc_converge` command. It is
intentionally a sanity check with loose tolerances; deeper fluid convergence
studies belong in examples or report workflows.

`dsmc-mass-flux-ablation-verification` runs a tiny fixed-sphere
chemistry-driven ablation step and checks final `mf` and `vf`. It is a compact
workflow regression, not a rough-sample proof. The rough-sample full chemistry
loop lives with the pregen TIFF carbon tests above.
