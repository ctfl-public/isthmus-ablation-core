# `isthmus_surface` Command

The `isthmus_surface` command reconstructs a triangle surface from active
voxels.

## Syntax

```text
isthmus_surface <surface-id> voxels <model> [resolution <R|A:B|voxel>] [iso <value>] [weighting yes|no] [map yes|no] [remove_sealed_pores yes|no] [crop real|no]
```

## Example

```text
isthmus_surface skin voxels solid iso 0.45
isthmus_surface skin voxels solid crop real
isthmus_surface skin voxels solid resolution 1.6 iso 0.6
```

## Description

The command sends the active voxels in `<model>` to ISTHMUS, runs marching
cubes, stores the resulting triangle mesh, and optionally stores
triangle-to-voxel ownership fractions.

Surface-to-voxel mapping is enabled by default because ablation needs ownership
fractions to send triangle mass loss back to voxels. Use `map no` only for
visualization or SPARTA-install surfaces that will not feed
`voxel_ablate <model> surface <surface-id>`.

Sealed internal pore surfaces are removed from the generated mesh by default.
This is the recommended behavior for DSMC/SPARTA ablation surfaces, where
sealed pores are unreachable by particles and can confuse SPARTA cut-cell
inside/outside classification. Use `remove_sealed_pores no` only for
non-DSMC uses where enclosed porosity is real data.

`buffer` is a legacy option accepted for input compatibility. Newer ISTHMUS
builds derive the required marching-domain padding internally, so new inputs
should omit it and use `iso` when an inward or outward surface shift is needed.

`resolution` controls the marching-cubes grid spacing relative to the voxel
spacing. The default is `voxel`, equivalent to `1:1`, meaning one marching cell
per voxel width. A value such as `2:1` makes the marching-cubes cell spacing
twice the voxel spacing. Numeric values are accepted as shorthand, so
`resolution 2` is equivalent to `resolution 2:1`. Larger values are coarser.

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
