# Testing

The project uses CTest.

Build and run tests:

```bash
cmake --preset standalone
cmake --build --preset standalone
ctest --preset standalone --output-on-failure
```

CTest captures output from passing tests by default. To see the stats table
while the test runs, use verbose CTest output:

```bash
ctest --preset standalone --output-on-failure --verbose
```

or use the convenience target:

```bash
cmake --build --preset standalone --target check-verbose
```

To build the optional visual verification report:

```bash
cmake --build --preset report
```

This runs the tests, collects each configured test report CSV, and writes:

```text
build/output/test-report.pdf
```

To select a subset directly:

```bash
python3 tools/run-test-report.py all
python3 tools/run-test-report.py 1-3
python3 tools/run-test-report.py slab-direct-command-verification
```

The standalone regression tests are:

```text
slab-direct-command-verification
slab-direct-yhi-verification
slab-isthmus-finite-surface-verification
slab-isthmus-ghost-wall-verification
sphere-isthmus-local-deletion-verification
sphere-isthmus-normal-carryover-verification
sphere-isthmus-kinetic-theory-verification
sphere-isthmus-normal-carryover-convergence
```

These are standalone/core tests. They run with the `ia-core` binary and should
be the only tests present in a normal standalone build.

To add coupled DSMC tests, configure and build the DSMC/IAC overlay:

```bash
cmake --preset dsmc
cmake --build --preset dsmc
ctest --preset dsmc
```

That build runs the standalone/core tests plus:

```text
dsmc-slab-direct-ablation-verification
dsmc-sphere-isthmus-normal-carryover-verification
dsmc-sphere-flux-verification
```

The DSMC tests are intentionally fast. The flux verification is a one-step
instantaneous kinetic-theory check: it verifies the initial O2 reaction count
and equivalent carbon mass flux before the perfectly consuming surface depletes
the nearby O2 population. Longer DSMC depletion and coupled-ablation studies
remain available as examples and report targets, but they are not part of the
automatic CTest suite.

To build the DSMC convergence report:

```bash
cmake --build --preset dsmc --target dsmc-convergence-report
```

It writes:

```text
build-dsmc/output/dsmc-sphere-kinetic-convergence/summary.csv
build-dsmc/output/dsmc-sphere-kinetic-convergence/report.pdf
```

The standalone test report currently includes:

```text
tests/inputs/slab-direct-ablation/in.slab-direct-command.verify
tests/inputs/slab-direct-ablation/in.slab-direct-yhi.verify
tests/inputs/slab-isthmus-ablation/in.slab-isthmus-finite-surface.verify
tests/inputs/slab-isthmus-ablation/in.slab-isthmus-ghost-wall.verify
tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-local-deletion.verify
tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-normal-carryover.verify
tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-kinetic-theory.verify
tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-normal-carryover-convergence.verify
```

The command-loop test includes the example case:

```text
include ../../../examples/slab-direct-ablation/in.slab-direct-ablation
```

The fix test keeps the compact callback-style path covered while the examples
move toward explicit `voxel ablate` loops. Tests pass only if all `verify`
commands in the wrapper input files pass.

The sphere ISTHMUS tests are enabled when the build finds the ISTHMUS C++
package. They run marching cubes, apply constant triangle flux, map that flux
back to voxels, and compare the inferred radius and mass fraction against the
continuum shrinking-sphere solution. The local sphere test keeps the
nonconservative path covered. The normal-carryover sphere test checks the
conservative path and verifies that final-step dropped mass is essentially zero.

The slab ISTHMUS tests compare a finite 4 by 4 surface patch against the same
patch with y/z infinite-wall ghost voxels. The finite-patch case has broad
tolerances because edge effects are expected. The ghost case should match the
one-dimensional slab recession nearly exactly.

The convergence test is an input file, not an external runner. It uses
`variable ... equal ...` and a `convergence ... vary ... order ...` command to
run normal carryover at 5, 10, and 20 voxels across the diameter with
mass-Courant timing. It requires monotone radius-error reduction. The
voxelized volume-fraction check allows non-monotone coarse stair-step ties but
still checks the end-to-end apparent order in a broad first-order band.

## Test Organization

Use `tests/inputs/` for automated regression wrappers. Keep these compact and
deterministic. Prefer including a file from `examples/` and adding only the
`verify` commands or test-specific criteria in the wrapper.

Use `examples/` for readable user-facing examples. Examples should describe and
run the physical case without embedding every regression criterion.

Example cases should use DSMC/SPARTA-style descriptive folders and input names:

```text
examples/<case-name>/in.<case-name>
tests/inputs/<case-name>/in.<case-name>.verify
```

Standalone convergence tests should live under `tests/inputs/` and use the
explicit `convergence` verification command. Coupled DSMC convergence tests can
use a runner under `tools/` when they need to launch DSMC repeatedly and
generate per-case input files under `build/output`.
