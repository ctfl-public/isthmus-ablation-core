# `voxel` Command

The `voxel` command family creates and configures voxel-state objects.

## Syntax

```text
voxel_material <name> density <rho> [molar-mass <kg/mol>] [formula <formula>]
voxel_create <model> slab nx <nx> ny <ny> nz <nz> dx <dx> material <name>
voxel_create <model> sphere diameter <D> dx <dx> material <name>
voxel_create <model> sphere diameter <D> resolution <N> material <name>
voxel_create <model> tiff file <path> dx <dx> material <name> \
  [ox <x>] [oy <y>] [oz <z>] [axes <xyz|...>] [origin <corner|center>]
voxel_write_history <model> <path>
```

## Examples

```text
voxel_material carbon density 1800.0 molar-mass 0.0120107 formula C
voxel_create solid slab nx 8 ny 4 nz 4 dx 1.0e-6 material carbon
voxel_create solid sphere diameter 1.0e-3 dx 5.0e-5 material carbon
voxel_create solid sphere diameter 1.0e-3 resolution 20 material carbon
voxel_create solid tiff file examples/tiff-sphere/sphere-24.tif dx 5.0e-5 material carbon
voxel_create solid tiff file slice.tiff dx 5.4934e-6 material carbon \
  ox 0.0 oy -0.007935216 oz -2.472e-5 axes xyz origin center
voxel_ghost solid axis y boundary infinite layers 1
voxel_write_history solid output/dsmc-sphere-kinetic/history.csv
```

## Description

`voxel_material` defines a material and its density. `molar-mass` and `formula`
are optional for purely mechanical voxel ablation, but are used by DSMC-coupled
surface-flux commands that infer solid mass consumption from a SPARTA
surface-reaction file.

`voxel_create` creates a named voxel model. Each voxel receives a stable ID,
integer lattice indices, a centroid, an initial mass based on density and
`dx^3`, and an active flag.

`slab` fills the full rectangular lattice.

`sphere` fills voxel centroids inside a sphere with diameter `D`. Specify either
`dx` or `resolution`, but not both. `resolution <N>` means `N` voxels across the
sphere diameter, so `dx = D/N`. Voxels outside the sphere remain in the bounding
lattice as inactive cells so the structured indexing and voxel IDs stay stable
while the sphere ablates.

`tiff` imports a multipage TIFF stack through ISTHMUS's TIFF utility. The
current shared utility supports a narrow uncompressed 8-bit grayscale stack
convention, with voxels whose image value is `1` treated as active. By default,
IAC interprets TIFF columns as `x`, rows as `y`, and image pages as `z`, which
matches the usual `width x height x stack-depth` convention. `ox`, `oy`, and
`oz` set the physical origin used to place the imported stack.

`axes` customizes how TIFF stack axes map into the IAC/DSMC domain. The three
letters name the TIFF axis used for IAC `x`, then IAC `y`, then IAC `z`.
The default is `axes xyz`. The older IAC behavior was equivalent to `axes zyx`,
where TIFF pages became IAC `x`, rows stayed IAC `y`, and columns became IAC
`z`.

`origin` controls how `ox`, `oy`, and `oz` are interpreted. The default is
`origin center`, where the values identify the first voxel center or lattice
point and the first voxel centroid sits at `origin`. Use `origin corner` when
the values are lower voxel-corner coordinates; then the first voxel centroid
sits at `origin + 0.5*dx`. `origin-mode` is accepted as an alias for `origin`.

`voxel_ghost` configures ghost voxel images used during surface generation.
The first supported boundary is `boundary infinite`, which extrapolates active
boundary voxels beyond the requested side of the requested axis. The optional
`side` value is `both` by default and may be `lo` or `hi` for one-sided cut
boundaries. Ghost voxels do not carry independent mass; their surface ownership
maps back to the corresponding real voxel. This is useful for making a finite
slab or scan patch behave like an infinite wall during ISTHMUS marching cubes.

`voxel_write_history` writes the current in-memory history table immediately.
Standalone inputs usually use `voxel_dump <id> <model> history ...`; the direct
write command is mainly for DSMC-hosted scripts, where DSMC owns the loop and
the core history should be flushed after the coupled loop completes.

## Current Limits

- Only one voxel model is currently supported.
- Only one material is currently supported.
- Slab, centroid-filled sphere, and ISTHMUS-backed narrow 8-bit TIFF-stack
  geometry are currently implemented.
- Ghost voxels currently support `boundary infinite`.

## Planned Extensions

- CSV import.
- Periodic and open ghost voxel behavior.
