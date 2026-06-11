# Testing

The clean full test path is the DSMC preset. It builds the core library,
standalone executable, private DSMC/IAC executable, and then runs the full
CTest matrix:

```bash
cmake --preset dsmc
cmake --build --preset dsmc
ctest --preset dsmc --output-on-failure
```

For a completely fresh rebuild:

```bash
rm -rf build build-dsmc
cmake --preset dsmc
cmake --build --preset dsmc
ctest --preset dsmc --output-on-failure
```

Inspect the registered tests without running them:

```bash
ctest --test-dir build-dsmc -N
```

The DSMC preset includes:

- standalone `ia-core` regression tests;
- the same pure-IAC inputs hosted through `build-dsmc/bin/dsmc-iac`;
- MPI-hosted twins when CMake finds an MPI launcher;
- DSMC gas-domain checks for surface flux measurement, bridge behavior, and a
  small particle-driven kinetic sphere recession convergence case.

For standalone-only development:

```bash
cmake --preset standalone
cmake --build --preset standalone
ctest --preset standalone --output-on-failure
```

CTest captures output from passing tests by default. To see stats output while
tests run:

```bash
ctest --preset standalone --output-on-failure --verbose
```

or:

```bash
cmake --build --preset standalone --target check-verbose
```

More detail on test categories, input-file conventions, and pass/fail criteria
lives in `tests/README.md`.

## Test Reports

The optional visual verification report is separate from the default test run:

```bash
cmake --build --preset report
```

This runs the configured report cases, collects verification CSV files, and
writes:

```text
build/output/test-report.pdf
```

To select a subset directly:

```bash
python3 tools/run-test-report.py all
python3 tools/run-test-report.py 1-3
python3 tools/run-test-report.py slab-direct-command-verification
```

Keep slow exploratory plotting and convergence reports out of the default CTest
suite until they are deterministic and fast enough to serve as regression
tests.

The coupled DSMC kinetic recession regression is intentionally small:

```bash
ctest --test-dir build-dsmc -R dsmc-sphere-kinetic-convergence --output-on-failure
```

It runs three generated DSMC inputs at 4, 6, and 8 voxels across the sphere
diameter and checks that the particle-sampled recession converges toward the
ideal-gas kinetic-theory mf solution.

The rough carbon TIFF workflow checks can be run directly with:

```bash
ctest --test-dir build -R pregen-tiff-carbon-recession-constant-flux-verification --output-on-failure
ctest --test-dir build-dsmc -R pregen-tiff-carbon-recession-dsmc-co-verification --output-on-failure
```

The first uses a prescribed top-surface mass flux. The second uses DSMC surface
chemistry (`O2 --> CO + CO`) to drive the same kind of voxel recession on the
same generated rough TIFF fixture.
