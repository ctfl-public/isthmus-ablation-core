# Testing

The project uses CTest.

Build and run tests:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

CTest captures output from passing tests by default. To see the stats table
while the test runs, use verbose CTest output:

```bash
ctest --test-dir build --output-on-failure --verbose
```

or use the convenience target:

```bash
cmake --build build --target check-verbose
```

To build the optional visual verification report:

```bash
cmake --build build --target test-report
```

This runs the tests, collects each configured test report CSV, and writes:

```text
build/output/test-report.pdf
```

To select a subset directly:

```bash
python3 tools/run-test-report.py all
python3 tools/run-test-report.py 1-2
python3 tools/run-test-report.py slab-local-verification
```

The current regression test is:

```text
slab-local-verification
slab-local-fix-verification
```

They run:

```text
tests/inputs/slab-local-ablation/in.slab-local-ablation.verify
tests/inputs/slab-local-ablation/in.slab-local-ablation.fix-verify
```

The command-loop test includes the example case:

```text
include ../../../examples/slab-local-ablation/in.slab-local-ablation
```

The fix test keeps the compact callback-style path covered while the examples
move toward explicit `voxel ablate` loops. Tests pass only if all `verify`
commands in the wrapper input files pass.

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

Future convergence tests should live under `tests/inputs/` and use an explicit
convergence verification command once implemented.
