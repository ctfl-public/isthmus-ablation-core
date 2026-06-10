# Verification

Verification should be input-driven, not hardcoded in the core model.

The core tracks actual quantities such as:

- `remaining-mass`
- `mass-fraction`
- `volume-fraction`
- `front`
- `active-voxels`
- `deleted-voxels`

The input file declares exact or reference expressions:

```text
verify mass-fraction exact "1.0 - q1*time/(rho*length)" tolerance 0.01 percent norm max
verify volume-fraction exact "1.0 - q1*time/(rho*length)" tolerance 0.01 percent norm final
verify front exact "q1*time/rho" tolerance 0.01 percent norm final
verify radius exact "initial-radius - q1*time/rho" tolerance 3.0 percent norm final
```

This keeps geometry-specific exact solutions out of the source code.

`mass-fraction` is the smooth remaining-mass ledger normalized by initial mass.
`volume-fraction` is the active voxel count normalized by the initial active
voxel count. It changes only when voxels are deleted, so it captures the
stair-stepped voxelized geometry response.

## Norms

`norm final` checks only the last history row.

`norm max` checks the maximum absolute error over recorded history rows.

`norm rms` checks the root-mean-square error over recorded history rows.

Regression tests should prefer percent tolerances unless an absolute error
scale is more meaningful.

## Convergence

Convergence checks are input-driven too. A test can define variables, use them
inside the case setup, and ask the executable to rerun the same input with
overrides:

```text
variable resolution equal 20
variable steps equal 4
voxel_create solid sphere diameter 1.0e-3 resolution ${resolution} material carbon
variable i loop ${steps}

convergence radius exact "initial-radius - q1*time/rho" tolerance 100.0 percent norm final vary resolution 5 10 20 vary steps 1 2 4 order 0.75 2.5 monotonic yes
convergence volume-fraction exact "((initial-radius - q1*time/rho)/initial-radius)^3" tolerance 100.0 percent norm final vary resolution 5 10 20 vary steps 1 2 4 order 0.75 2.5 monotonic no
```

This keeps convergence criteria in the test input instead of a separate helper
script.
