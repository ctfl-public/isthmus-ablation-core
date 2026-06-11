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
`mflux-last` and `selected` cell fields, so the full surface can be colored
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
../../build-dsmc/bin/dsmc-iac -log output/pregen-tiff-carbon-recession/log.sparta -in in.dsmc-co
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
before mapping reaction counts back to the voxels. The input uses SPARTA's
`run ... every 25 "iac_spa_stats"` hook to print compact `[SPA]` gas-domain
progress during the 100-step sample without spelling out a second input loop.
The installed SPARTA surface is the full watertight ISTHMUS surface, but the
ablation flux is restricted to triangles facing `x+` with `select normal nx 1.0
ny 0.0 nz 0.0 min-cos 0.5`.
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

The `fluid_*.vtu` files are written from a SPARTA `fix ave/grid` and include
cell temperature, pressure, and O2/N2/CO mass fractions. The input uses
`grid_write_vtu` once to write `fluid_000000.vtu`, then uses scheduled
`grid_dump` output with `index iac-step` so later fluid files share the same
ablation-step numbering as the voxel and surface files. It also writes
`dsmc-surface_000000.vtp` before the coupling loop so ParaView can open the
fluid, voxel, and surface series together from the same initial index.

The first compact output block is an `[IAC]` configuration summary covering the
voxel model and the DSMC/SPARTA setup: domain bounds, boundary conditions,
species/mixtures, surface chemistry, and fluid dump paths. After that,
`iac_spa_stats` prints SPARTA gas statistics as a fixed-width `[SPA]` table
during each gas sample, followed by IAC voxel ledger statistics with an `[IAC]`
prefix after the solid update.

## Regression Tests

The standalone constant-flux wrapper runs without visual dumps:

```bash
ctest --test-dir build -R pregen-tiff-carbon-recession-constant-flux-verification --output-on-failure
```

The DSMC chemistry wrapper runs the same generated sample through the DSMC/IAC
executable:

```bash
ctest --test-dir build-dsmc -R pregen-tiff-carbon-recession-dsmc-co-verification --output-on-failure
```
