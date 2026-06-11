# `iac_verify` Command

The `iac_verify` command compares tracked quantities to input-file
exact/reference expressions.

## Syntax

```text
iac_verify <quantity> exact "<expression>" tolerance <value> [absolute|percent] norm <final|max|rms>
convergence <quantity> exact "<expression>" tolerance <value> [absolute|percent] norm <final|max|rms> vary <variable> <v1> <v2> ... [vary <variable> <v1> <v2> ...] order <min> <max> [monotonic yes|no]
```

## Examples

```text
iac_verify remaining-mass exact "initial-mass - q1*area*time" tolerance 0.01 percent norm max
iac_verify mass-fraction exact "1.0 - q1*time/(rho*length)" tolerance 0.01 percent norm max
iac_verify volume-fraction exact "1.0 - q1*time/(rho*length)" tolerance 0.01 percent norm final
iac_verify front exact "q1*time/rho" tolerance 0.01 percent norm final
iac_verify radius exact "initial-radius - q1*time/rho" tolerance 3.0 percent norm final
convergence radius exact "initial-radius - q1*time/rho" tolerance 100.0 percent norm final vary resolution 5 10 20 vary steps 1 2 4 order 0.75 2.5 monotonic yes
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

## Convergence

`convergence` reruns the same input file with variable overrides and checks the
error trend. Variables are defined with:

```text
variable resolution equal 20
variable steps equal 4
```

and referenced with `${resolution}` or `${steps}` elsewhere in the input.

Each `vary` clause must have the same number of values. The first `vary`
variable is treated as the refinement variable for apparent-order calculation.
For example, `vary resolution 5 10 20 vary steps 1 2 4` runs three cases:

```text
resolution=5,  steps=1
resolution=10, steps=2
resolution=20, steps=4
```

The convergence check requires monotone error reduction by default and checks
that the apparent order from the first to last case is inside the requested
`order <min> <max>` band.

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
volume-fraction
voxel-volume-fraction
front
q1
source:q1
```
