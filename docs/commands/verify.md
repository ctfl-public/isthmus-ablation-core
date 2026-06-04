# `verify` Command

The `verify` command compares tracked quantities to input-file exact/reference
expressions.

## Syntax

```text
verify <quantity> exact "<expression>" tolerance <value> [absolute|percent] norm <final|max|rms>
```

## Examples

```text
verify remaining-mass exact "initial-mass - q1*area*time" tolerance 0.01 percent norm max
verify mass-fraction exact "1.0 - q1*time/(rho*length)" tolerance 0.01 percent norm max
verify front exact "q1*time/rho" tolerance 0.01 percent norm final
verify radius exact "initial-radius - q1*time/rho" tolerance 3.0 percent norm final
```

## Description

The command evaluates an exact expression against the selected tracked quantity.
The executable exits with a nonzero status if the error exceeds the tolerance.
This makes verification cases natural CTest tests.

Regression tests generally use percent tolerances. Add `percent` after the
tolerance value to check:

```text
100 * abs(actual - exact) / abs(exact)
```

Percent error is undefined when the exact value is zero unless the actual value
is also zero.

Use `absolute` or omit the mode when an absolute tolerance is more appropriate.

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
