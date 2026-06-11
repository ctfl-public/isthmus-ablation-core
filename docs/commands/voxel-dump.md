# `voxel_dump` Command

The `voxel_dump` command writes voxel-model output.

Triangle surface dumps are configured with [`surf_dump`](surface.md).

## Syntax

```text
voxel_dump <id> <model> history <N> <path>
voxel_dump <id> <model> vtu <N> <path> [select all|active|ghosted|ghosts] [scalar <field>]
voxel_dump off

# Also available in the DSMC bridge:
voxel_dump <id> <model> history <N> <path>
voxel_dump <id> <model> vtu <N> <path> [select all|active|ghosted|ghosts] [scalar <field>]
voxel_write_vtu <model> <path> [select all|active|ghosted|ghosts] [scalar <field>]
```

## Examples

```text
voxel_dump hist solid history 1 output/slab-direct-ablation/history.csv
voxel_dump vox solid vtu 1 output/slab-direct-ablation/voxels_*.vtu select active scalar mf
voxel_dump off

voxel_write_vtu solid output/voxels-*.vtu select active scalar mf
```

## Description

The `history` style writes one CSV summary after the run.

`voxel_dump off` clears voxel dumps that were already defined. This is useful
in regression wrappers that include a visual example input but should avoid
writing output files during CTest.

The `vtu` style writes VTK unstructured-grid files during the run. A dump is
written at step 0, every `N` steps, and at the final step. If the path contains
`*`, that character is replaced by a zero-padded step number:

```text
output/slab-direct-ablation/voxels_000004.vtu
```

If the path does not contain `*`, the step number is inserted before the file
extension.

The `select` option controls which voxels are written:

- `select all` writes every voxel, including inactive/deleted voxels.
- `select active` writes only active voxels.
- `select ghosted` writes active real voxels plus the ghost-image voxels used
  for ISTHMUS surface generation. Ghost images are marked with cell data
  `ghost = 1` and do not contribute to mass totals.
- `select ghosts` writes only ghost-image voxels. This is the preferred visual
  mode when an example needs to show boundary ghosts separately from the real
  solid.

The default is `select active`, so depleted voxels disappear from VTU output
when the ablation fix uses `delete yes`.

The `scalar` option sets the active VTK cell scalar. This is the field ParaView
should choose first when the file opens. Supported values are:

```text
mf
mass
active
fixed
ghost
id
ix
iy
iz
```

The default is `scalar mf`.

Inside DSMC, scheduled `voxel_dump` output is written whenever a bridge command
advances the core step, for example after `voxel_ablate`. This is useful for
no-gas verification and coupled loops that use explicit DSMC input-file
commands for ablation updates.

`voxel_write_vtu` writes a one-shot VTU snapshot immediately. This is useful in
coupled loops where the input script decides exactly when a voxel snapshot
should be written. It uses the same `select` and `scalar` options as scheduled
`voxel_dump ... vtu` output. If the path does not contain `*`, the current core
step number is inserted before the file extension.

## History Columns

```text
step,time,nvox,ndel,mass,mf,
vf,front,rad,mreq,mapp,
mdrop
```

## VTU Cell Data

The VTU dump writes hexahedral voxel cells with:

```text
mf
mass
id
ix
iy
iz
active
fixed
ghost
```

Use `active` for thresholding deleted voxels and `mf` or
`mass` for coloring ablation progress in ParaView. Use `ghost` to
hide or recolor mirror voxels that were emitted for boundary inspection.

## Planned Extensions

- Restart dumps.
- Material/field output.
