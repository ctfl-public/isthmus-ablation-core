# `iac_stats` and `iac_stats_style` Commands

The `iac_stats` and `iac_stats_style` commands control IAC console output.

## Syntax

```text
iac_stats <N>
iac_stats_style <column> <column> ...
```

## Example

```text
iac_stats 1
iac_stats_style step time active-voxels deleted-voxels remaining-mass mass-fraction volume-fraction front
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
active-voxels
deleted-voxels
remaining-mass
mass-fraction
volume-fraction
radius
front
requested-mass-step
applied-mass-step
dropped-mass-step
```

## DSMC/SPARTA Note

In DSMC-hosted runs, use `iac_stats` and `iac_stats_style` for the core's own
ablation table:

```text
iac_stats 1
iac_stats_style step time active-voxels deleted-voxels remaining-mass mass-fraction radius
```

These commands do not change DSMC's native `stats` or `stats_style` settings.
For coupled gas/solid runs, DSMC can continue printing particle/collision
statistics while `iac_stats` prints the voxel mass ledger after IAC ablation
steps. Longer term, selected IAC quantities can also be exposed through DSMC
compute/fix styles for native `c_...` or `f_...` output.
