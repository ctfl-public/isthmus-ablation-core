# `voxel` Command

The `voxel` command family creates and configures voxel-state objects.

## Syntax

```text
voxel material <name> density <rho>
voxel create <model> slab nx <nx> ny <ny> nz <nz> dx <dx> material <name>
```

## Examples

```text
voxel material carbon density 1800.0
voxel create solid slab nx 8 ny 2 nz 2 dx 1.0e-6 material carbon
```

## Description

`voxel material` defines a material and its density.

`voxel create` creates a named voxel model. The current implementation supports
only `slab` geometry. Each voxel receives a stable ID, integer lattice indices,
an initial mass based on density and `dx^3`, and an active flag.

## Current Limits

- Only one voxel model is currently supported.
- Only one material is currently supported.
- Only slab geometry is currently implemented.

## Planned Extensions

- Sphere geometry.
- TIFF import.
- CSV import.
- Boundary and ghost voxel behavior.

