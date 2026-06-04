# `verify` Command

The `verify` command compares tracked quantities to input-file exact/reference
expressions.

## Syntax

```text
verify <quantity> exact "<expression>" tolerance <value> norm <final|max|rms>
```

## Examples

```text
verify remaining-mass exact "initial-mass - q1*area*time" tolerance 1.0e-27 norm max
verify mass-fraction exact "1.0 - q1*time/(rho*length)" tolerance 1.0e-14 norm max
verify front exact "q1*time/rho" tolerance 1.0e-6 norm max
```

## Description

The command evaluates an exact expression against the selected tracked quantity.
The executable exits with a nonzero status if the error exceeds the tolerance.
This makes verification cases natural CTest tests.

## Norms

`final` checks the last history row.

`max` checks the maximum absolute error over all recorded rows.

`rms` checks the root-mean-square error over all recorded rows.

## Expression Variables

```text
time
t
step
dt
rho
density
dx
nx
ny
nz
length
area
initial-mass
voxel-mass
active-voxels
deleted-voxels
remaining-mass
mass-fraction
front
q1
source:q1
```

