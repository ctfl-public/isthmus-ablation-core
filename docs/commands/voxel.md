# `voxel` Command

The `voxel` command family creates and configures voxel-state objects.

## Syntax

```text
voxel material <name> density <rho>
voxel create <model> slab nx <nx> ny <ny> nz <nz> dx <dx> material <name>
voxel create <model> sphere diameter <D> dx <dx> material <name>
voxel create <model> sphere diameter <D> resolution <N> material <name>
```

## Examples

```text
voxel material carbon density 1800.0
voxel create solid slab nx 8 ny 2 nz 2 dx 1.0e-6 material carbon
voxel create solid sphere diameter 1.0e-3 dx 5.0e-5 material carbon
voxel create solid sphere diameter 1.0e-3 resolution 20 material carbon
```

## Description

`voxel material` defines a material and its density.

`voxel create` creates a named voxel model. Each voxel receives a stable ID,
integer lattice indices, a centroid, an initial mass based on density and
`dx^3`, and an active flag.

`slab` fills the full rectangular lattice.

`sphere` fills voxel centroids inside a sphere with diameter `D`. Specify either
`dx` or `resolution`, but not both. `resolution <N>` means `N` voxels across the
sphere diameter, so `dx = D/N`. Voxels outside the sphere remain in the bounding
lattice as inactive cells so the structured indexing and voxel IDs stay stable
while the sphere ablates.

## Current Limits

- Only one voxel model is currently supported.
- Only one material is currently supported.
- Slab and centroid-filled sphere geometry are currently implemented.

## Planned Extensions

- TIFF import.
- CSV import.
- Boundary and ghost voxel behavior.
