# Getting Started

Build the standalone executable:

```bash
cmake -S . -B build
cmake --build build
```

Run the first example:

```bash
./build/ia-core -in examples/slab-direct-ablation/in.slab-direct-ablation
```

Run the regression tests:

```bash
ctest --test-dir build --output-on-failure
```

The default build runs standalone/core tests only. To include coupled DSMC
tests, configure with a DSMC executable:

```bash
cmake -S . -B build-dsmc \
  -DIAC_DSMC_EXECUTABLE=/Users/tstoffel1/dsmc/src/spa_mac_mpi
cmake --build build-dsmc
ctest --test-dir build-dsmc --output-on-failure
```

CTest prints output from failing tests by default. To also see the aligned stats
table for passing tests:

```bash
cmake --build build --target check-verbose
```

Rebuild the PDF manual:

```bash
cmake --build build --target docs-pdf
```

The PDF is written to:

```text
docs/isthmus-ablation-core-manual.pdf
```

If CMake has not been configured yet, run the full sequence:

```bash
cmake -S . -B build
cmake --build build --target docs-pdf
```

The example writes a CSV history file:

```text
output/slab-direct-ablation/history.csv
```

It also writes VTU voxel files for visual inspection:

```text
output/slab-direct-ablation/voxels_000000.vtu
output/slab-direct-ablation/voxels_000001.vtu
...
```

The example writes active voxels only and marks `mass-fraction` as the active
ParaView cell scalar.

When run through CTest, the test writes its output under the CTest working
directory:

```text
build/output/slab-direct-ablation/history.csv
```
