# `grid_write_vtu` Command

The `grid_write_vtu` command writes DSMC grid-cell data from a SPARTA
`fix ave/grid` object to a ParaView-ready VTU file.

```text
grid_write_vtu <fix-id> <path> [index dsmc-step|iac-step|iac-next] fields <name> ...
```

The number of field names must match the number of columns produced by the
fix. If `<path>` contains `*`, the selected index is inserted as a zero-padded
number.

Index modes:

- `dsmc-step` uses the native DSMC timestep. This is the default, so files are
  named like `fluid_000100.vtu`, `fluid_000200.vtu`, and so on.
- `iac-step` uses the current IAC solid-ablation step.
- `iac-next` uses the next IAC solid-ablation step. This is usually the best
  choice when the command is placed after `run` and before `surf_flux`,
  `voxel_ablate`, and `iac_run 1`, because the fluid file then lines up with
  the voxel and surface files produced by that ablation step.

Example:

```text
compute        gas thermal/grid all air temp press
compute        frac grid all species massfrac
fix            gasave ave/grid all 1 100 100 c_gas[*] c_frac[*] ave one
run            100 post no
grid_write_vtu gasave output/fluid_*.vtu index iac-next fields temperature pressure mass-fraction-O2 mass-fraction-N2 mass-fraction-CO
```

This command is DSMC-hosted only. It is intended for visualizing coupled runs,
where DSMC owns gas particles, grid cells, sampling, and mixture properties.
It writes cell fields, not individual particles; pressure, temperature, and
species mass fractions are statistical grid quantities.

If multiple solid ablation updates are performed before another DSMC `run`,
the same sampled fluid fields can be written again with new `iac-next` indices.
That gives a coalesced ablation-step view while keeping DSMC sampling explicit
in the input file.
