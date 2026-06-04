# `timestep` Command

The `timestep` command sets the standalone time increment.

## Syntax

```text
timestep <dt>
timestep mass/courant <C> source <source-id>
```

## Examples

```text
timestep 1.0e-6
timestep mass/courant 0.5 source q1
```

## Description

The explicit form sets `dt` directly.

The mass-Courant form chooses:

```text
dt = C * rho * dx / source
```

For the current slab case, `C = 0.5` removes half of one voxel mass from each
exposed column per step.

