# `voxel_ablate` Command

The `voxel_ablate` command applies one timestep of mass loss to a voxel model.

## Syntax

```text
voxel_ablate <model> source <source-id> policy <policy> face <face> delete <yes|no>
voxel_ablate <model> surface <surface-id> policy <policy> delete <yes|no>
```

## Example

```text
voxel_ablate solid source q1 policy local face xlo delete yes
voxel_ablate solid source q1 policy local face yhi delete yes
voxel_ablate solid surface skin policy local delete yes
voxel_ablate solid surface skin policy carryover/normal delete yes
```

## Description

The command applies one mass-loss update over the current timestep.

With `source`, `policy local` removes mass from the first active voxel in each
slab column on the selected face. The required `face` argument accepts `xlo`,
`xhi`, `ylo`, `yhi`, `zlo`, or `zhi`.

With `surface`, the command consumes mass already assigned to triangles by
`surf_flux`, uses the ISTHMUS triangle-to-voxel ownership map, and subtracts
that mass from the associated voxels.

`policy local` removes only the mass available in each mapped voxel and records
any overshoot as dropped mass.

`policy carryover/normal` conserves overshoot by pushing excess mass loss from
a depleted voxel into live 26-neighbor voxels along the inward normal direction.
This policy is intended for closed ISTHMUS surfaces such as spheres.

Use `iac_run 1` after `voxel_ablate` to advance IAC solid time, record history,
print stats, and write scheduled dumps:

```text
voxel_ablate solid source q1 policy local face xlo delete yes
voxel_ablate solid source q1 policy local face zhi delete yes
iac_run 1
```

A surface-coupled loop is:

```text
isthmus_surface skin voxels solid buffer 3 map yes
surf_flux skin source q1 select all
voxel_ablate solid surface skin policy carryover/normal delete yes
iac_run 1
```

`delete yes` is the default. When a voxel's mass is depleted, it is marked
inactive and omitted from VTU dumps that use `select active`.

## Current Limits

- Only one voxel model is currently supported.
- `policy carryover/normal` is currently implemented for surface-backed
  ablation.
- The current source must be a constant mass flux.
