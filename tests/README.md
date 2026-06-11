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
| DSMC gas-domain checks | `dsmc-*` and `dsmc-mpi-*` | Actual DSMC setup, surface installation, surface flux measurement, and bridge verification work in serial and MPI. |

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
