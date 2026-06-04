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
stats_style step time active-voxels deleted-voxels remaining-mass mass-fraction front
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
front
requested-mass-step
applied-mass-step
dropped-mass-step
```

## DSMC/SPARTA Note

In coupled DSMC/SPARTA runs, SPARTA should own its native `stats_style`.
This project should expose values through compute/fix styles that SPARTA can
reference with `c_...` or `f_...`.

