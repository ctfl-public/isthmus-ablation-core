# isthmus-ablation-core

`isthmus-ablation-core` is a C++ voxel ablation and coupling core. It owns
solid voxel state, tracks mass and material properties, supports standalone
verification runs, and is being shaped so DSMC/SPARTA can call it through thin
commands, fixes, and computes.

The project requires ISTHMUS. ISTHMUS provides surface reconstruction,
surface-to-voxel mapping, and the shared TIFF import utility. The core is still
more than an ISTHMUS wrapper: it owns voxel bookkeeping, local voxel-face
ablation, generated geometries, exact-solution verification, and DSMC coupling
state.

## Quick Start

If DSMC and ISTHMUS are already installed, set their roots once:

```bash
export DSMC_ROOT=$HOME/dsmc
export ISTHMUS_ROOT=$HOME/isthmus
```

Then build a private DSMC/IAC executable from this repository:

```bash
cmake --preset dsmc
cmake --build --preset dsmc
ctest --preset dsmc
```

The executable is:

```text
build-dsmc/bin/dsmc-iac
```

This does not edit the DSMC checkout. The build creates a disposable overlay in
`build-dsmc/dsmc-overlay/`, where DSMC source files and IAC bridge commands are
symlinked together for compilation.

Run a coupled example:

```bash
build-dsmc/bin/dsmc-iac \
  -in examples/dsmc-sphere-kinetic/in.dsmc-sphere-kinetic
```

For standalone-only work:

```bash
cmake --preset standalone
cmake --build --preset standalone
ctest --preset standalone
```

## Current Capabilities

- Create a slab voxel model.
- Import a small ISTHMUS-backed TIFF stack as voxels.
- Generate a deterministic rough carbon TIFF sample for lightweight examples
  and tests.
- Assign material density.
- Define a constant mass-loss source.
- Use explicit or mass-Courant timesteps.
- Apply local voxel ablation with delete-empty behavior.
- Use simple standalone loops with `variable`, `label`, `next`, and `jump`.
- Print aligned standalone stats with `iac_stats` and `iac_stats_style`.
- Write a CSV history file.
- Write VTU voxel files for visual inspection.
- Compute single-species ideal-gas kinetic-theory flux on ISTHMUS triangles.
- Read DSMC `nflux_incident` surface tallies and convert them to voxel mass
  loss through ISTHMUS triangle ownership.
- Read DSMC surface-reaction tallies and convert CO-forming reactions into
  carbon voxel mass loss.
- Build a private DSMC executable that calls the core through thin
  command-style bridge files without modifying the DSMC checkout.
- Include shared input files with `include`.
- Verify tracked quantities against input-file exact expressions.
- Optionally build visual verification reports from test data.

## Rebuild Docs

Documentation source lives in `docs/`. After changing command syntax,
examples, concepts, or behavior, rebuild the single PDF manual:

```bash
cmake -S . -B build
cmake --build build --target docs-pdf
```

The generated manual is:

```text
docs/isthmus-ablation-core-manual.pdf
```

You can also call the local PDF builder directly:

```bash
python3 tools/build-docs-pdf.py
```

## Current Run Type

The standalone executable currently prints:

```text
# Standalone voxel ablation
```

Future ISTHMUS-backed runs should use a more specific title, such as:

```text
# Coupled voxel/ISTHMUS surface ablation
```

## First Example

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

The regression wrapper in
`tests/inputs/slab-direct-ablation/in.slab-direct-command.verify` includes this
example and adds exact-solution checks with `iac_verify`.
