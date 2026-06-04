# Command Reference

The command language is intentionally close to DSMC/SPARTA style: one command
per line, simple positional arguments, and keyword/value options.

New command names avoid underscores where this project controls the syntax.
Existing DSMC/SPARTA commands keep their native spelling, for example
`stats_style` and `surf_modify`.

Current standalone commands:

- [`include`](include.md)
- [`voxel`](voxel.md)
- [`source`](source.md)
- [`timestep`](timestep.md)
- [`fix ... voxel/ablate`](fix-voxel-ablate.md)
- [`stats` and `stats_style`](stats.md)
- [`voxel dump`](voxel-dump.md)
- [`verify`](verify.md)
- [`run`](run.md)
