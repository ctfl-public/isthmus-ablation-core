# `voxel dump` Command

The `voxel dump` command writes voxel-model output.

Triangle surface dumps are configured with [`surface dump`](surface.md).

## Syntax

```text
voxel dump <id> <model> history <N> <path>
voxel dump <id> <model> vtu <N> <path> [select all|active] [scalar <field>]
voxel dump off

# DSMC bridge only:
voxel write-vtu <model> <path> [select all|active] [scalar <field>]
```

## Examples

```text
voxel dump hist solid history 1 output/slab-direct-ablation/history.csv
voxel dump vox solid vtu 1 output/slab-direct-ablation/voxels_*.vtu select active scalar mass-fraction
voxel dump off

voxel write-vtu solid output/voxels-*.vtu select active scalar mass-fraction
```

## Description

The `history` style writes one CSV summary after the run.

`voxel dump off` clears voxel dumps that were already defined. This is useful
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

The default is `select active`, so depleted voxels disappear from VTU output
when the ablation fix uses `delete yes`.

The `scalar` option sets the active VTK cell scalar. This is the field ParaView
should choose first when the file opens. Supported values are:

```text
mass-fraction
remaining-mass
active
fixed
id
ix
iy
iz
```

The default is `scalar mass-fraction`.

Inside DSMC, `voxel write-vtu` writes a one-shot VTU snapshot immediately. This
is useful in coupled loops where DSMC owns the timestep and the input script
decides when a voxel snapshot should be written. It uses the same `select` and
`scalar` options as scheduled `voxel dump ... vtu` output. If the path does not
contain `*`, the current core step number is inserted before the file extension.

## History Columns

```text
step,time,active-voxels,deleted-voxels,remaining-mass,mass-fraction,
volume-fraction,front,radius,requested-mass-step,applied-mass-step,
dropped-mass-step
```

## VTU Cell Data

The VTU dump writes hexahedral voxel cells with:

```text
mass-fraction
remaining-mass
id
ix
iy
iz
active
fixed
```

Use `active` for thresholding deleted voxels and `mass-fraction` or
`remaining-mass` for coloring ablation progress in ParaView.

## Planned Extensions

- Restart dumps.
- Material/field output.
