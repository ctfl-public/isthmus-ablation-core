# `voxel` Command

The `voxel` command family creates and configures voxel-state objects.

## Syntax

```text
voxel material <name> density <rho> [molar-mass <kg/mol>] [formula <formula>]
voxel create <model> slab nx <nx> ny <ny> nz <nz> dx <dx> material <name>
voxel create <model> sphere diameter <D> dx <dx> material <name>
voxel create <model> sphere diameter <D> resolution <N> material <name>
voxel create <model> tiff file <path> dx <dx> threshold <value> \
  [invert yes|no] material <name> [ox <x>] [oy <y>] [oz <z>]
voxel write-history <model> <path>
```

## Examples

```text
voxel material carbon density 1800.0 molar-mass 0.0120107 formula C
voxel create solid slab nx 8 ny 4 nz 4 dx 1.0e-6 material carbon
voxel create solid sphere diameter 1.0e-3 dx 5.0e-5 material carbon
voxel create solid sphere diameter 1.0e-3 resolution 20 material carbon
voxel create solid tiff file examples/tiff-carbon-sample/carbon-sample-top-crop.tif \
  dx 3.3757e-6 threshold 1 invert yes material carbon
voxel ghost solid axis y boundary infinite layers 1
voxel write-history solid output/dsmc-sphere-kinetic/history.csv
```

## Description

`voxel material` defines a material and its density. `molar-mass` and `formula`
are optional for purely mechanical voxel ablation, but are used by DSMC-coupled
surface-flux commands that infer solid mass consumption from a SPARTA
surface-reaction file.

`voxel create` creates a named voxel model. Each voxel receives a stable ID,
integer lattice indices, a centroid, an initial mass based on density and
`dx^3`, and an active flag.

`slab` fills the full rectangular lattice.

`sphere` fills voxel centroids inside a sphere with diameter `D`. Specify either
`dx` or `resolution`, but not both. `resolution <N>` means `N` voxels across the
sphere diameter, so `dx = D/N`. Voxels outside the sphere remain in the bounding
lattice as inactive cells so the structured indexing and voxel IDs stay stable
while the sphere ablates.

`tiff` imports a multipage TIFF stack as a structured voxel lattice. Each TIFF
directory becomes one voxel plane, scanline rows become the second lattice
direction, and image columns become the third lattice direction. The current
reader supports one-sample 8-bit and 16-bit TIFF data. Voxels with values
greater than or equal to `threshold` are active by default. Use `invert yes`
when the source image stores solid material as dark pixels, as in the committed
`carbon-sample-top-crop.tif` example. `ox`, `oy`, and `oz` optionally shift the
voxel-centroid origin.

`voxel ghost` configures ghost voxel images used during surface generation.
The first supported boundary is `boundary infinite`, which extrapolates active
boundary voxels beyond both ends of the requested axis. Ghost voxels do not
carry independent mass; their surface ownership maps back to the corresponding
real voxel. This is useful for making a finite slab patch behave like an
infinite wall during ISTHMUS marching cubes.

`voxel write-history` writes the current in-memory history table immediately.
Standalone inputs usually use `voxel dump <id> <model> history ...`; the direct
write command is mainly for DSMC-hosted scripts, where DSMC owns the loop and
the core history should be flushed after the coupled loop completes.

## Current Limits

- Only one voxel model is currently supported.
- Only one material is currently supported.
- Slab, centroid-filled sphere, and 8/16-bit single-sample TIFF-stack geometry
  are currently implemented.
- Ghost voxels currently support `boundary infinite`.

## Planned Extensions

- CSV import.
- Periodic and open ghost voxel behavior.
