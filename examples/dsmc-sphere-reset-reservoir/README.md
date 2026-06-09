# DSMC Sphere Reset Reservoir

This example runs a coupled DSMC/voxel/ISTHMUS sphere recession problem while
imposing a fresh, spatially uniform gas reservoir at every ablation update.
Each update deletes the current particles, recreates the hot air reservoir,
runs one DSMC step to sample surface chemistry, maps the reacting `O2` flux
back to voxels, ablates with normal carryover, deletes depleted voxels, and
regenerates the ISTHMUS surface.

The case is intentionally a diagnostic rather than a polished validation case.
It removes gas-depletion history from the problem, so disagreement with the
smooth analytical sphere recession mostly points at voxel/surface topology,
normal carryover, and deep-remesh behavior.

Build the DSMC overlay from the repository root, then run from this directory
with:

```sh
../../build-dsmc/bin/dsmc-iac -screen none -log output/log.sparta -in in.dsmc-sphere-reset-reservoir
```

The history file is written to `output/history.csv`. The checked case uses 100
coupling updates and reaches about 28 percent remaining mass on the current
local build.
