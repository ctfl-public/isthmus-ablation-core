# Expected Features

This page tracks planned user-visible capabilities. It is intentionally more
stable than planning notes and brainstorm documents.

## Voxel Core

- Generate slab, sphere, cylinder/cone, and imported voxel geometries.
- Track stable voxel IDs, lattice coordinates, material, remaining mass,
  activity, fixed state, and future fields.
- Support local ablation, column carryover, axial carryover, normal-directed
  carryover, ghost voxels, and configurable boundary behavior.
- Dump history, VTU voxel state, restart state, and diagnostic summaries.

## Ablation Control

The long-term design should support both explicit commands and timestep
callbacks. We will implement whichever path is easiest first while keeping the
core API usable by both.

The command-oriented form is useful for readable standalone scripts and
DSMC/SPARTA loops:

```text
label ablate-loop
voxel ablate solid source q1 policy local delete yes
isthmus surface surf1 voxels solid map yes
surface flux surf1 source dsmc-tallies select all
voxel ablate solid surface surf1 policy local delete yes
jump SELF ablate-loop
```

Standalone inputs can loop over these commands directly. DSMC/SPARTA inputs can
use native variables, labels, jumps, and repeated `run` chunks to couple gas
evolution and voxel/surface updates.

The callback form may use a fix-style or other timestep hook to call the same
core operations during a longer DSMC/SPARTA run.

## ISTHMUS Coupling

- Generate a surface from active voxels using ISTHMUS.
- Cache triangle IDs, areas, normals, and triangle-to-voxel ownership
  fractions.
- Map surface quantities back to voxel mass loss.
- Regenerate the surface when voxel state changes.
- Export VTP and SPARTA `.surf` files in standalone mode.
- Install or replace explicit DSMC/SPARTA surfaces in coupled mode.

## DSMC/SPARTA Coupling

- Let DSMC/SPARTA own particle advancement, collisions, reactions, MPI,
  variables, loops, and stats.
- Read per-surface DSMC tallies after a `run` interval.
- Map those tallies to ISTHMUS triangles and then to voxels.
- Apply voxel mass loss with chosen policies.
- Regenerate and reinstall the surface before the next `run` interval.
