# Local Slab Ablation

This example removes exactly half of one voxel mass from each exposed slab
column per step.

Input file:

```text
units si

voxel material carbon density 1800.0
voxel create solid slab nx 8 ny 2 nz 2 dx 1.0e-6 material carbon

source q1 constant 1.8 units kg/m2/s
timestep mass/courant 0.5 source q1

# Compact callback-style alternative:
# fix ab all voxel/ablate 1 voxels solid source q1 policy local delete yes

stats 1
stats_style step time active-voxels deleted-voxels remaining-mass mass-fraction front

voxel dump hist solid history 1 output/slab-local-ablation/history.csv
voxel dump vox solid vtu 1 output/slab-local-ablation/voxels_*.vtu select active scalar mass-fraction

variable i loop 8
label ablate-loop
voxel ablate solid source q1 policy local delete yes
run 1
next i
jump SELF ablate-loop
```

Run it:

```bash
./build/ia-core -in examples/slab-local-ablation/in.slab-local-ablation
```

The example writes:

```text
output/slab-local-ablation/history.csv
output/slab-local-ablation/voxels_000000.vtu
output/slab-local-ablation/voxels_000001.vtu
...
output/slab-local-ablation/voxels_000008.vtu
```

The VTU dump uses `select active`, so voxels removed by `delete yes` disappear
from the visualized voxel mesh. It also uses `scalar mass-fraction`, so
ParaView should open the files with `mass-fraction` as the active cell field.

The test form is:

```text
include ../../../examples/slab-local-ablation/in.slab-local-ablation

verify remaining-mass exact "initial-mass - q1*area*time" tolerance 0.01 percent norm max
verify mass-fraction exact "1.0 - q1*time/(rho*length)" tolerance 0.01 percent norm max
verify front exact "q1*time/rho" tolerance 0.01 percent norm final
```

There is also a fix-driven regression input that keeps the commented compact
path covered:

```text
tests/inputs/slab-local-ablation/in.slab-local-ablation.fix-verify
```

Run the test:

```bash
ctest --test-dir build --output-on-failure
```

Print the stats table during the test:

```bash
cmake --build build --target check-verbose
```

Build the visual verification report:

```bash
cmake --build build --target test-report
```

The combined report is written to:

```text
build/output/test-report.pdf
```

To report only this test, run:

```bash
python3 tools/run-test-report.py slab-local-verification \
  --out build/output/slab-local-report.tex
```
