# Verification

Verification should be input-driven, not hardcoded in the core model.

The core tracks actual quantities such as:

- `remaining-mass`
- `mass-fraction`
- `front`
- `active-voxels`
- `deleted-voxels`

The input file declares exact or reference expressions:

```text
verify mass-fraction exact "1.0 - q1*time/(rho*length)" tolerance 1.0e-14 norm max
verify front exact "q1*time/rho" tolerance 1.0e-6 norm max
```

This keeps geometry-specific exact solutions out of the source code.

## Norms

`norm final` checks only the last history row.

`norm max` checks the maximum absolute error over recorded history rows.

`norm rms` checks the root-mean-square error over recorded history rows.

## Convergence

Grid and time convergence checks are planned. They should build on the same
verification machinery rather than adding special-case exact solution branches
to the model.

