# `iac_stats` and `iac_stats_style` Commands

The `iac_stats` and `iac_stats_style` commands control IAC console output.

## Syntax

```text
iac_stats <N>
iac_stats_style <column> <column> ...
iac_spa_stats
```

## Example

```text
iac_stats 1
iac_stats_style step time nvox ndel mass mf vf front
```

## Description

`iac_stats N` prints every `N` IAC steps and always prints step 0 and the final
step.

`iac_stats_style` selects columns from tracked history quantities. The
standalone runner prints a title/configuration block first, then a fixed-width
iac_stats table.

## Current Columns

```text
step
time
nvox
ndel
mass
mf
vf
rad
front
mreq
mapp
mdrop
```

## DSMC/SPARTA Note

In DSMC-hosted runs, use `iac_stats` and `iac_stats_style` for the core's own
ablation table:

```text
iac_stats 1
iac_stats_style step time nvox ndel mass mf rad
```

These commands do not change DSMC's native `stats` or `stats_style` settings.
For coupled gas/solid runs, DSMC can continue owning particle/collision
statistics while `iac_stats` prints the voxel mass ledger after IAC ablation
steps.

`iac_spa_stats` is a DSMC-hosted helper for coupled logs. Place it after a
native DSMC `run` and before the IAC ablation update:

```text
stats_style     step np nscoll nscheck nsreact
stats           100000
run             100 post no
iac_spa_stats
voxel_ablate    solid surface skin policy carryover/normal delete yes
iac_run         1
```

On the first call, the bridge prints an `[IAC]` title/configuration block before
the first `[SPA]` table. The block summarizes the voxel model, DSMC box,
boundary conditions, timestep, particle weight, species/mixtures, surface
collision/reaction models, and registered `grid_write_vtu` fluid outputs.

Each call then captures SPARTA's current native `stats_style` header and row,
formats them as a fixed-width `[SPA]` table, and prints them at the gas-to-solid
switch point. It does not modify the user's native `stats_style` command.
