# Command Reference

The command language is intentionally close to DSMC/SPARTA style: one command
per line, simple positional arguments, keyword/value options, and underscore
separated top-level command names. IAC commands use explicit prefixes such as
`iac_`, `voxel_`, `isthmus_`, and `surf_` so they can live beside native DSMC
commands like `create_grid`, `surf_modify`, `stats_style`, and `run`.

Current IAC commands:

- [`include`](include.md)
- [`voxel_material`, `voxel_create`, and `voxel_ghost`](voxel.md)
- [`source`](source.md)
- [`iac_timestep`](timestep.md)
- [`variable`, `label`, `next`, and `jump`](loops.md)
- [`voxel_ablate`](voxel-ablate.md)
- [`isthmus_surface`](isthmus-surface.md)
- [`surf_flux`, `surf_dump`, `surf_install`, `surf_measure_flux`, and `surf_write_vtp`](surface.md)
- [`grid_write_vtu`](grid-write-vtu.md)
- [`iac_limit`, `iac_continue`, and `iac_set`](iac.md)
- [`iac_stats` and `iac_stats_style`](stats.md)
- [`voxel_dump`](voxel-dump.md)
- [`iac_verify`](verify.md)
- [`iac_run`](run.md)
