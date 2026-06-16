# Direct Slab Ablation

This example removes exactly half of one voxel mass from each exposed slab
column per step.

Input file:

```text
units si

voxel_material carbon density 1800.0
voxel_create solid slab nx 8 ny 4 nz 4 dx 1.0e-6 material carbon

source q1 constant 1.8 units kg/m2/s
iac_timestep mass/courant 0.5 source q1
variable ablation_time equal 4.0e-3
variable keep internal 1

iac_stats 1
iac_stats_style step time nvox ndel mass mf vf front

voxel_dump hist solid history 1 output/slab-direct-ablation/history.csv
voxel_dump vox solid vtu 1 output/slab-direct-ablation/voxels_*.vtu select active scalar mf

label ablate-loop
iac_limit time ${ablation_time}
voxel_ablate solid source q1 policy local face xlo delete yes
iac_run 1
iac_continue time ${ablation_time} variable keep
if "${keep} > 0" then "jump SELF ablate-loop"
```

Run it:

```bash
./build/ia-core -in examples/slab-direct-ablation/in.slab-direct-ablation
```

The example writes:

```text
output/slab-direct-ablation/history.csv
output/slab-direct-ablation/voxels_000000.vtu
output/slab-direct-ablation/voxels_000001.vtu
...
output/slab-direct-ablation/voxels_000008.vtu
```

The VTU dump uses `select active`, so voxels removed by `delete yes` disappear
from the visualized voxel mesh. It also uses `scalar mf`, so
ParaView should open the files with `mf` as the active cell field.

The test form is:

```text
include ../../../examples/slab-direct-ablation/in.slab-direct-ablation

iac_verify mass exact "mass0 - q1*area*time" tolerance 0.01 percent norm max
iac_verify mf exact "1.0 - q1*time/(rho*length)" tolerance 0.01 percent norm max
iac_verify front exact "q1*time/rho" tolerance 0.01 percent norm final
```

Run the test:

```bash
make test-standalone
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
python3 tools/run-test-report.py slab-direct-command-verification \
  --out build/output/slab-direct-report.tex
```
