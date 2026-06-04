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

The current regression test is:

```text
slab-local-verification
```

It runs:

```text
tests/inputs/slab-local-ablation/in.slab-local-ablation.verify
```

That file includes the example case:

```text
include ../../../examples/slab-local-ablation/in.slab-local-ablation
```

The test passes only if all `verify` commands in the wrapper input file pass.

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
