# Pregen TIFF Carbon Recession

This example folder contains two small rough-carbon recession cases built from
the same deterministic TIFF stack:

```text
examples/pregen-tiff-carbon-recession/
  generate-sample.sh
  in.constant-flux
  in.dsmc-co
  air.species
  air.vss
  carbon-co.surf
```

The generated sample is mostly solid carbon with shallow random pits at the top
surface. It is intentionally synthetic and reproducible, so the repository does
not need to carry a binary scan file.

## Generate the TIFF

From the repository root:

```bash
examples/pregen-tiff-carbon-recession/generate-sample.sh
```

This writes:

```text
examples/pregen-tiff-carbon-recession/carbon-sample.tif
```

The default sample is `24 x 24 x 10` voxels and contains `4650 / 5760` active
voxels. The generated TIFF is ignored by Git.

## Constant-Flux Case

Run the standalone case from the example folder:

```bash
cmake --preset standalone
cmake --build --preset standalone
cd examples/pregen-tiff-carbon-recession
./generate-sample.sh
./clean-output.sh
../../build/ia-core -in in.constant-flux
```

The case imports the TIFF, places infinite-wall ghost voxels on the four lateral
sides, supports the `x-lo` side with one-sided ghost voxels, reconstructs the
exposed pitted `x-hi` surface with ISTHMUS, applies a constant mass flux along
`+x`, and ablates with normal-directed carryover:

```text
voxel_ghost         solid axis y boundary infinite layers 1
voxel_ghost         solid axis z boundary infinite layers 1
voxel_ghost         solid axis x side lo boundary infinite layers 1
surf_flux           skin source qtop select normal nx 1.0 ny 0.0 nz 0.0 min-cos 0.5
voxel_ablate        solid surface skin policy carryover/normal delete yes
```

The example writes real voxels and ghost voxels as separate VTU series. The
ghost files use `select ghosts`, so ParaView can show the mirror-image ghost
voxels used by ISTHMUS without mixing them into the real solid. The surface VTP
files dump the full closed real-plus-ghost ISTHMUS surface. They include
`last-mass-flux` and `selected` cell fields, so the full surface can be colored
by applied flux while unselected triangles remain visible with zero flux.

With the committed parameters, the case recesses to about `0.70` remaining
mass fraction and `0.75` remaining voxelized volume fraction. This makes it a
standalone sister case to the DSMC chemistry example below.

It writes visual output under the example folder:

```text
output/pregen-tiff-carbon-recession/constant-history.csv
output/pregen-tiff-carbon-recession/constant-voxels_*.vtu
output/pregen-tiff-carbon-recession/constant-ghosts_*.vtu
output/pregen-tiff-carbon-recession/constant-surface_*.vtp
```

## DSMC CO-Chemistry Case

Run the coupled DSMC case from its folder so the local species and reaction
files resolve naturally:

```bash
cmake --preset dsmc
cmake --build --preset dsmc
cd examples/pregen-tiff-carbon-recession
./generate-sample.sh
./clean-output.sh
../../build-dsmc/bin/dsmc-iac -screen none -log log.sparta -in in.dsmc-co
```

This case installs the full watertight ISTHMUS surface into DSMC/SPARTA,
creates a hot pure-O2 stream at 8000 K entering from the `xhi` face toward the
sample's pitted `xhi` surface, applies the SPARTA-style surface reaction

```text
O2 --> CO + CO
D S 1.0
```

and maps `compute react/surf` reaction counts back to carbon voxels through the
ISTHMUS triangle-to-voxel map. It is a compact workflow example, not a
high-fidelity oxidation validation case; gas-gas collisions are intentionally
omitted to keep it cheap.

The chemistry-driven flux is sampled from `compute react/surf` and the solid
timestep is selected with `mass-courant 1.0`, so the example advances
through several small voxel-ablation updates instead of consuming the full
sampled reaction mass in one step. Each coupling update samples 100 DSMC steps
before mapping reaction counts back to the voxels. The installed SPARTA surface
is the full watertight ISTHMUS surface, but the ablation flux is restricted to
triangles facing `x+` with `select normal nx 1.0 ny 0.0 nz 0.0 min-cos 0.5`.
The committed input runs to `2.0e-7 s`, reaches about `0.70` remaining mass
fraction and `0.75` voxelized volume fraction, and stops before the remaining
rough geometry reaches the current SPARTA watertight-check limit.

It writes visual output under:

```text
output/pregen-tiff-carbon-recession/dsmc-history.csv
output/pregen-tiff-carbon-recession/dsmc-voxels_*.vtu
output/pregen-tiff-carbon-recession/dsmc-ghosts_*.vtu
output/pregen-tiff-carbon-recession/dsmc-surface_*.vtp
output/pregen-tiff-carbon-recession/fluid_*.vtu
```

The `fluid_*.vtu` files are written by `grid_write_vtu` from a SPARTA
`fix ave/grid` and include cell temperature, pressure, and O2/N2/CO mass
fractions. The input uses `index iac-next`, so these files are numbered by the
ablation step they feed rather than by the native DSMC timestep. It also writes
`fluid_000000.vtu` and `dsmc-surface_000000.vtp` before the coupling loop so
ParaView can open the fluid, voxel, and surface series together from the same
initial index.

## Regression Tests

The standalone constant-flux wrapper runs without visual dumps:

```bash
ctest --test-dir build -R tiff-carbon-sample-constant-verification --output-on-failure
```

The DSMC chemistry wrapper runs the same generated sample through the DSMC/IAC
executable:

```bash
ctest --test-dir build-dsmc -R dsmc-tiff-carbon-co-verification --output-on-failure
```
