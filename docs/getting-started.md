# Getting Started

Build the standalone executable:

```bash
cmake -S . -B build
cmake --build build
```

Run the first example:

```bash
./build/ia-core -in examples/slab-local-ablation/in.slab-local-ablation
```

Run the regression tests:

```bash
ctest --test-dir build --output-on-failure
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
output/slab-local-ablation/history.csv
```

It also writes VTU voxel files for visual inspection:

```text
output/slab-local-ablation/voxels_000000.vtu
output/slab-local-ablation/voxels_000001.vtu
...
```

The example writes active voxels only and marks `mass-fraction` as the active
ParaView cell scalar.

When run through CTest, the test writes its output under the CTest working
directory:

```text
build/output/slab-local-ablation/history.csv
```
