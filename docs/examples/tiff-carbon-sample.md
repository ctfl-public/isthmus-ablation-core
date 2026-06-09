# TIFF Carbon Sample Examples

These examples demonstrate importing a small multipage TIFF crop from the
carbon sample into the voxel ledger. The committed fixture is:

```text
examples/tiff-carbon-sample/carbon-sample-top-crop.tif
```

It is a deliberately small crop so the examples stay cheap. The crop imports as
a `12 x 24 x 23` voxel lattice with `dx = 3.3757e-6 m` and 6504 active voxels.
The source TIFF stores solid material as dark palette values, so the inputs use:

```text
voxel create solid tiff file carbon-sample-top-crop.tif \
  dx 3.3757e-6 threshold 1 invert yes material carbon
```

## Standalone Constant-Flux Recession

Run from the repository root:

```bash
cmake --preset standalone
cmake --build --preset standalone
./build/ia-core -in examples/tiff-carbon-sample/in.tiff-carbon-sample-constant
```

The case imports the TIFF, adds infinite-wall ghost voxels on the lateral
directions, reconstructs an ISTHMUS surface, applies a constant flux on
triangles whose normals face the exposed end, maps the mass loss back to
voxels, and deletes depleted voxels:

```text
voxel ghost solid axis y boundary infinite layers 1
voxel ghost solid axis z boundary infinite layers 1
isthmus surface skin voxels solid buffer 1 weighting no map yes crop real
surface flux skin source q1 select normal nx -1.0 ny 0.0 nz 0.0 min-cos 0.25
voxel ablate solid surface skin policy local delete yes
```

The input is tuned as a visual example rather than a regression test. It runs
quickly and reaches about 50 percent remaining mass in 12 ablation updates.

Outputs are written under:

```text
output/tiff-carbon-sample-constant/
```

Open the `voxels_*.vtu` and `surface_*.vtp` files in ParaView.

## DSMC CO-Formation Coupled Example

Build the DSMC overlay, then run from the example directory:

```bash
cmake --preset dsmc
cmake --build --preset dsmc

cd examples/tiff-carbon-sample
../../build-dsmc/bin/dsmc-iac -in in.tiff-carbon-sample-dsmc-co
```

This case imports the same TIFF crop, installs the ISTHMUS surface into DSMC,
creates hot O2/N2 gas, and uses SPARTA surface chemistry to form CO:

```text
surf_react          ox prob carbon-co.surf
compute             rco react/surf all ox
fix                 rco ave/surf all 1 20 20 c_rco[*] ave one
surface flux skin dsmc/reaction fix rco column 1 sample-steps 20 \
  solid-mass-per-reaction 3.98894696e-26 time-scale 500
voxel ablate solid surface skin policy carryover/normal delete yes
```

The DSMC parameters are intentionally modest so the example usually runs in a
few seconds on a laptop. It is a workflow and visualization case, not a tight
continuum verification case. The committed DSMC flux tests remain the right
place for quantitative reaction-flux checks.

The rough imported crop can be awkward for DSMC cut-cell construction when
cropped back to the real domain. This example keeps the lateral ghost extension
in the installed surface, which makes the mesh easier for DSMC to cut while
still mapping ghost-surface ownership back to the real voxels.

The case writes:

```text
examples/tiff-carbon-sample/output/tiff-carbon-sample-dsmc-co-history.csv
examples/tiff-carbon-sample/output/voxels-*.vtu
examples/tiff-carbon-sample/output/surface-*.vtp
```

The surface VTP files include `co-reaction-count`, and the voxel VTU files
write active voxels with `mass-fraction` as the visible scalar.
