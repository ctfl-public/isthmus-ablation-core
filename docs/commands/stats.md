# `stats` and `stats_style` Commands

The `stats` and `stats_style` commands control standalone console output.

## Syntax

```text
stats <N>
stats_style <column> <column> ...
```

## Example

```text
stats 1
stats_style step time active-voxels deleted-voxels remaining-mass mass-fraction volume-fraction front
```

## Description

`stats N` prints every `N` steps and always prints step 0 and the final step.

`stats_style` selects columns from tracked history quantities. The standalone
runner prints a title/configuration block first, then a fixed-width stats table.

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

In DSMC-hosted runs, use `iac stats` and `iac stats-style` for the core's own
ablation table:

```text
iac stats 1
iac stats-style step time active-voxels deleted-voxels remaining-mass mass-fraction radius
```

These commands do not change DSMC's native `stats` or `stats_style` settings.
For coupled gas/solid runs, DSMC can continue printing particle/collision
statistics while `iac stats` prints the voxel mass ledger after IAC ablation
steps. Longer term, selected IAC quantities can also be exposed through DSMC
compute/fix styles for native `c_...` or `f_...` output.
