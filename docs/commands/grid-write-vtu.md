# `grid_write_vtu` and `grid_dump` Commands

The `grid_write_vtu` and `grid_dump` commands write DSMC grid-cell data from a
SPARTA `fix ave/grid` object to ParaView-ready VTU files.

```text
grid_write_vtu <fix-id> <path> [index dsmc-step|iac-step|iac-next] fields <name> ...
grid_dump <dump-id> <fix-id> vtu <N> <path> [index dsmc-step|iac-step|iac-next] fields <name> ...
```

The number of field names must match the number of columns produced by the
fix. If `<path>` contains `*`, the selected index is inserted as a zero-padded
number.

`grid_write_vtu` is a one-shot command: it writes the current grid-fix data
immediately. It is useful for writing `fluid_000000.vtu` before a coupled
ablation loop starts.

`grid_dump` is scheduled output. It registers a grid VTU dump and writes it
after IAC solid-ablation steps whose step count is divisible by `<N>`. This is
the fluid analog of `voxel_dump` and `surf_dump`.

Index modes:

- `dsmc-step` uses the native DSMC timestep. This is the default, so files are
  named like `fluid_000100.vtu`, `fluid_000200.vtu`, and so on.
- `iac-step` uses the current IAC solid-ablation step.
- `iac-next` uses the next IAC solid-ablation step. This is useful for one-shot
  `grid_write_vtu` calls placed immediately before an `iac_run`.

Example:

```text
compute        gas thermal/grid all air temp press
compute        frac grid all species massfrac
fix            gasave ave/grid all 1 100 100 c_gas[*] c_frac[*] ave one
grid_write_vtu gasave output/fluid_*.vtu index iac-step fields temperature pressure mass-fraction-O2 mass-fraction-N2 mass-fraction-CO
grid_dump      fluid gasave vtu 1 output/fluid_*.vtu index iac-step fields temperature pressure mass-fraction-O2 mass-fraction-N2 mass-fraction-CO
```

This command is DSMC-hosted only. It is intended for visualizing coupled runs,
where DSMC owns gas particles, grid cells, sampling, and mixture properties.
It writes cell fields, not individual particles; pressure, temperature, and
species mass fractions are statistical grid quantities.

With this pattern, `grid_write_vtu` writes the initial `fluid_000000.vtu` and
`grid_dump` writes later files on the same IAC step index as the corresponding
voxel and surface dumps.
