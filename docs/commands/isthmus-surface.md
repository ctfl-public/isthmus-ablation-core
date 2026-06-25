# `isthmus_surface` Command

The `isthmus_surface` command reconstructs a triangle surface from active
voxels.

## Syntax

```text
isthmus_surface <surface-id> voxels <model> [buffer <N>] [resolution <R|A:B|voxel>] [iso <value>] [weighting yes|no] [map yes|no] [crop real|no]
```

## Example

```text
isthmus_surface skin voxels solid buffer 3 iso 0.45 map yes
isthmus_surface skin voxels solid buffer 3 map yes crop real
isthmus_surface skin voxels solid buffer 3 resolution 2:1 map yes
```

## Description

The command sends the active voxels in `<model>` to ISTHMUS, runs marching
cubes, stores the resulting triangle mesh, and optionally stores
triangle-to-voxel ownership fractions.

`map yes` is required when the surface will be used for ablation, because
`voxel_ablate <model> surface <surface-id>` needs those ownership fractions to
send triangle mass loss back to voxels.

`buffer` adds empty voxel layers around the active voxel bounding box before
marching cubes. This helps ISTHMUS generate a closed surface as the solid
shrinks.

`resolution` controls the marching-cubes grid spacing relative to the voxel
spacing. The default is `voxel`, equivalent to `1:1`, meaning one marching cell
per voxel width. A value such as `2:1` makes the marching-cubes cell spacing
twice the voxel spacing. Numeric values are accepted as shorthand, so
`resolution 2` is equivalent to `resolution 2:1`. Larger values are coarser and
usually need a larger `buffer` because each marching cell spans more voxel
layers.

`iso` sets the marching-cubes isovalue used by ISTHMUS. The default is `0.5`,
which extracts the middle of the scalar transition between solid and empty
space. Values must be between `0` and `1`. Lower values move the reconstructed
surface outward into the transition band, while higher values move it inward.
The aliases `isovalue`, `iso_value`, and `iso-value` are also accepted.

`weighting` controls ISTHMUS depth-based corner weighting. The default is
`weighting yes`, matching native ISTHMUS behavior. Use `weighting no` only when
you need the older unweighted reconstruction for comparison.

`crop real` removes triangles whose centroids are outside the real voxel domain
after ghost voxels are added. Use it when ghost voxels are only a boundary
condition for surface quality and flux should still be applied only on the real
DSMC/voxel domain.

## DSMC Coupling Note

This command is intentionally an explicit input-file command rather than a
hidden fix callback. A future DSMC loop can call it after voxel mass changes
and before refreshing the DSMC surface representation.
