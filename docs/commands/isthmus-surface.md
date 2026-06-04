# `isthmus surface` Command

The `isthmus surface` command reconstructs a triangle surface from active
voxels.

## Syntax

```text
isthmus surface <surface-id> voxels <model> [buffer <N>] [weighting yes|no] [map yes|no] [crop real|no]
```

## Example

```text
isthmus surface skin voxels solid buffer 1 weighting no map yes
isthmus surface skin voxels solid buffer 1 weighting no map yes crop real
```

## Description

The command sends the active voxels in `<model>` to ISTHMUS, runs marching
cubes, stores the resulting triangle mesh, and optionally stores
triangle-to-voxel ownership fractions.

`map yes` is required when the surface will be used for ablation, because
`voxel ablate <model> surface <surface-id>` needs those ownership fractions to
send triangle mass loss back to voxels.

`buffer` adds empty voxel layers around the active voxel bounding box before
marching cubes. This helps ISTHMUS generate a closed surface as the solid
shrinks.

`crop real` removes triangles whose centroids are outside the real voxel domain
after ghost voxels are added. Use it when ghost voxels are only a boundary
condition for surface quality and flux should still be applied only on the real
DSMC/voxel domain.

## DSMC Coupling Note

This command is intentionally an explicit input-file command rather than a
hidden fix callback. A future DSMC loop can call it after voxel mass changes
and before refreshing the DSMC surface representation.
