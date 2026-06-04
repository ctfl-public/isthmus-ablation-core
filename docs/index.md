# isthmus-ablation-core

`isthmus-ablation-core` is a C++ voxel ablation and coupling core. It owns
solid voxel state, tracks mass and material properties, supports standalone
verification runs, and is being shaped so DSMC/SPARTA can call it through thin
commands, fixes, and computes.

The project is strongly linked to ISTHMUS, but it is not only an ISTHMUS
wrapper. ISTHMUS is the surface reconstruction and surface-to-voxel mapping
backend. The core should also remain useful for voxel bookkeeping, local
voxel-face ablation, generated geometries, and exact-solution verification
without a fluid domain.

## Current Capabilities

- Create a slab voxel model.
- Assign material density.
- Define a constant mass-loss source.
- Use explicit or mass-Courant timesteps.
- Apply local voxel ablation with delete-empty behavior.
- Use simple standalone loops with `variable`, `label`, `next`, and `jump`.
- Print aligned standalone stats with `stats` and `stats_style`.
- Write a CSV history file.
- Write VTU voxel files for visual inspection.
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

voxel material carbon density 1800.0
voxel create solid slab nx 8 ny 2 nz 2 dx 1.0e-6 material carbon

source q1 constant 1.8 units kg/m2/s
timestep mass/courant 0.5 source q1

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

The regression wrapper in
`tests/inputs/slab-local-ablation/in.slab-local-ablation.verify` includes this
example and adds exact-solution checks with `verify`.
