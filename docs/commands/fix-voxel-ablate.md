# `fix ... voxel/ablate` Command

The `voxel/ablate` fix applies mass loss to a voxel model during standalone
time advancement. It is the compact callback-style alternative to explicit
`voxel ablate` commands.

## Syntax

```text
fix <id> all voxel/ablate <Nevery> voxels <model> source <source-id> policy <policy> face <face> delete <yes|no>
```

## Example

```text
fix ab all voxel/ablate 1 voxels solid source q1 policy local face xlo delete yes
fix ab all voxel/ablate 1 voxels solid source q1 policy local face yhi delete yes
```

## Description

Every `Nevery` steps, the fix applies the selected source to the selected voxel
model. The current implementation supports a local slab face update:

- find the first active voxel from the selected face in each slab column;
- remove `source * dx^2 * dt` from that voxel;
- delete the voxel if its mass reaches zero and `delete yes` is set.

The required `face` argument accepts `xlo`, `xhi`, `ylo`, `yhi`, `zlo`, or
`zhi`.

`delete yes` is the default behavior. When a voxel's mass is depleted, it is
marked inactive and omitted from VTU dumps that use `select active`. Use
`delete no` only for cases where depleted voxels should remain in the active
voxel set.

## Current Limits

- Only group `all` is accepted.
- Only policy `local` is implemented.
- Only slab face ablation is implemented.

## Planned Extensions

- Column carryover.
- Axial carryover.
- Normal-directed carryover.
- Shared core support for both explicit `voxel ablate` commands and fix-style
  timestep callbacks.
