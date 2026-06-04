# `surface` Commands

The `surface` command family works with triangle surfaces generated from
voxels. These commands are deliberately separate from `voxel` commands because
future DSMC-coupled runs will need to refresh surfaces, apply DSMC surface
tallies, and then map that information back to voxels inside input-file loops.

## Syntax

```text
surface flux <surface-id> source <source-id> select all
surface flux <surface-id> source <source-id> select normal nx <x> ny <y> nz <z> [min-cos <c>]
surface flux <surface-id> source <source-id> select voxels voxels <model>

surface dump <dump-id> <surface-id> vtp <N> <path>
surface dump off
```

## Examples

```text
surface flux skin source q1 select all
surface flux skin source q1 select normal nx 1.0 ny 0.0 nz 0.0 min-cos 0.5
surface flux skin source q1 select voxels voxels solid

surface dump skin skin vtp 10 output/sphere/surface_*.vtp
surface dump off
```

## Description

`surface flux` assigns one timestep of mass loss to selected triangles on an
existing surface. The source is currently a constant mass flux in `kg/m2/s`.
The requested mass on each selected triangle is:

```text
mass = flux * triangle-area * timestep
```

The mass is stored on the triangle until a later command consumes it:

```text
voxel ablate solid surface skin policy local delete yes
```

Selectors:

- `select all` applies flux to every triangle.
- `select normal` applies flux only when the triangle normal has
  `dot(normal, direction) >= min-cos`.
- `select voxels` applies flux to triangles that have ISTHMUS ownership
  entries for the named voxel model. This is a forward-compatible hook for
  voxel groups and material subsets.

`surface dump` writes VTP triangle files on scheduled run steps. The VTP cell
data includes `area`, `requested-mass`, and `last-requested-mass`. The
`last-requested-mass` field is useful for visual inspection because
`requested-mass` is cleared after `voxel ablate` consumes it.

`surface dump off` clears surface dumps that were already defined. Regression
wrappers can use it after including a visual example input.

## Current Limits

- Only VTP surface dumps are implemented.
- Only constant source flux is implemented.
- `select voxels` currently distinguishes ownership presence for the current
  voxel model, not separate voxel groups.
