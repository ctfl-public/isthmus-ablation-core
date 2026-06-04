# `voxel ablate` Command

The `voxel ablate` command applies one timestep of mass loss to a voxel model.

## Syntax

```text
voxel ablate <model> source <source-id> policy <policy> delete <yes|no>
```

## Example

```text
voxel ablate solid source q1 policy local delete yes
```

## Description

The command applies the selected source over the current timestep. In the
current implementation, `policy local` removes mass from the first active voxel
in each slab column.

Use `run 1` after `voxel ablate` to advance time, record history, print stats,
and write scheduled dumps:

```text
voxel ablate solid source q1 policy local delete yes
run 1
```

`delete yes` is the default. When a voxel's mass is depleted, it is marked
inactive and omitted from VTU dumps that use `select active`.

## Current Limits

- Only one voxel model is currently supported.
- Only `policy local` is implemented.
- The current source must be a constant mass flux.
