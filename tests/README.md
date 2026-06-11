# Tests

The default DSMC preset runs the useful test matrix:

```bash
cmake --preset dsmc
cmake --build --preset dsmc
ctest --preset dsmc --output-on-failure
```

Inspect registered tests without running them:

```bash
ctest --test-dir build-dsmc -N
```

## Test Groups

| Group | Names | What they prove |
| --- | --- | --- |
| Standalone IAC | `slab-*`, `sphere-*`, `tiff-*` | The core library, standalone parser, ISTHMUS path, ablation policies, dumps, and exact-solution checks work without DSMC. |
| DSMC-hosted parity | `hosted-*` | The same pure-IAC input files can run through the DSMC executable. |
| DSMC-hosted MPI parity | `hosted-mpi-*` | The same bridge commands do not break when DSMC launches with MPI. |
| DSMC gas-domain checks | `dsmc-*` and `dsmc-mpi-*` | Actual DSMC setup, surface installation, surface flux measurement, bridge verification, and coupled particle-driven recession work. |

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
