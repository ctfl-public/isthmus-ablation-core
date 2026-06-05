# `surface` Commands

The `surface` command family works with triangle surfaces generated from
voxels. These commands are deliberately separate from `voxel` commands because
future DSMC-coupled runs will need to refresh surfaces, apply DSMC surface
tallies, and then map that information back to voxels inside input-file loops.

## Syntax

```text
surface flux <surface-id> source <source-id> select all
surface flux <surface-id> source <source-id> select normal nx <x> ny <y> nz <z> [min-cos <c>]
surface flux <surface-id> source <source-id> select voxels voxels <model>
surface flux <surface-id> kinetic/theory pressure <p> temperature <T> \
  mole-fraction <x> molecular-mass <kg> reaction-prob <alpha> \
  solid-mass-per-hit <kg> select <selector>
surface flux <surface-id> dsmc/surf fix <fix-id> column <N> \
  reaction-prob <alpha> solid-mass-per-hit <kg> \
  [ablation-dt <dt> | mass-courant <C>]
surface flux <surface-id> dsmc/reaction fix <fix-id> column <N> \
  sample-steps <Nstep> solid-mass-per-reaction <kg> \
  [ablation-dt <dt>] [time-scale <S>]

surface dump <dump-id> <surface-id> vtp <N> <path>
surface dump off

# DSMC bridge only:
surface write-vtp <surface-id> <path> [fix <fix-id> fields <name1> ...]
```

## Examples

```text
surface flux skin source q1 select all
surface flux skin source q1 select normal nx 1.0 ny 0.0 nz 0.0 min-cos 0.5
surface flux skin source q1 select voxels voxels solid
surface flux skin kinetic/theory pressure 50.0 temperature 5000.0 \
  mole-fraction 0.21 molecular-mass 5.313e-26 reaction-prob 1.0 \
  solid-mass-per-hit 1.3011869411625376e-23 select all
surface flux skin dsmc/surf fix sflux column 1 reaction-prob 1.0 \
  solid-mass-per-hit 1.99447348e-26 mass-courant 0.25
surface flux skin dsmc/reaction fix rco column 1 sample-steps 20 \
  solid-mass-per-reaction 3.98894696e-26 time-scale 1500

surface dump skin skin vtp 10 output/sphere/surface_*.vtp
surface dump off

surface write-vtp skin output/surface-*.vtp \
  fix sqty fields collision-count incident-number-flux pressure
```

## Description

`surface flux` assigns one timestep of mass loss to selected triangles on an
existing surface. With `source`, the source is a constant mass flux in
`kg/m2/s`. The requested mass on each selected triangle is:

```text
mass = flux * triangle-area * timestep
```

With `kinetic/theory`, the command computes the mass flux from ideal-gas
impingement theory for one reactive species:

```text
Gamma = mole-fraction * pressure/(kB*temperature)
        * sqrt(kB*temperature/(2*pi*molecular-mass))
flux = reaction-prob * solid-mass-per-hit * Gamma
```

This is intended as the first DSMC-coupled verification source: DSMC can run a
spatially uniform, stationary hot gas domain while this command applies the
corresponding continuum kinetic-theory flux to the current ISTHMUS triangles.

With `dsmc/surf`, the command reads per-surface data from a DSMC `fix ave/surf`
instance. The selected column is interpreted as an incident number flux. The
bridge converts it to solid mass flux with:

```text
flux = number-flux * reaction-prob * solid-mass-per-hit
```

This is the first true DSMC-coupled source path. DSMC owns the gas domain,
surface collisions, surface averaging, loops, and remapping commands; this
library owns the voxel mass ledger, ISTHMUS surface ownership map, and ablation.
The current bridge implementation supports this source on one MPI rank while
the command and data path are kept compatible with later distributed storage.

With `dsmc/reaction`, the command reads per-surface reaction counts from a DSMC
`fix ave/surf`, usually one that averages `compute react/surf`. The selected
column is interpreted as an averaged number of simulation-particle reactions per
surface element per sampled timestep. The bridge converts it to real solid mass
removed over the coupling window with:

```text
mass = reaction-count * sample-steps * fnum * solid-mass-per-reaction
```

and then divides by triangle area and the ablation timestep before passing the
equivalent mass flux to the voxel ledger. This is the preferred path for
chemically reacting DSMC ablation because SPARTA owns the surface reaction
probability, species conversion, and reaction tallies.

By default, DSMC-coupled sources advance voxel ablation by the elapsed DSMC
time since the previous coupling update. The optional `ablation-dt` argument
overrides that physical ablation time. For `dsmc/reaction`, `time-scale`
multiplies both the sampled reaction-count mass and the ablation time. This is
useful for quasi-steady chemistry probes: the input can sample a short DSMC
window and advance the solid over a longer reservoir-equivalent ablation time.
When comparing multiple DSMC sampling lengths at the same ablation time, choose
`time-scale = ablation-update-time / (sample-steps * timestep)`.

The optional `mass-courant` argument chooses the ablation timestep from the
sampled DSMC mass flux. The bridge maps the triangle mass fluxes through the
ISTHMUS triangle-to-voxel ownership fractions, finds the active voxel with the
smallest `remaining-mass / mapped-mass-rate`, and sets the timestep to `C`
times that limiting time. For example, `mass-courant 1.0` advances to the next
voxel-deletion event predicted by the current flux map, while
`mass-courant 0.25` advances one quarter of the way to that event.
`ablation-dt` and `mass-courant` are mutually exclusive.

The mass is stored on the triangle until a later command consumes it:

```text
voxel ablate solid surface skin policy local delete yes
```

Selectors:

- `select all` applies flux to every triangle.
- `select normal` applies flux only when the triangle normal has
  `dot(normal, direction) >= min-cos`.
- `select voxels` applies flux to triangles that have ISTHMUS ownership
  entries for the named voxel model. This is a forward-compatible hook for
  voxel groups and material subsets.

`surface dump` writes VTP triangle files on scheduled run steps. The VTP cell
data includes `area`, `requested-mass`, and `last-requested-mass`. The
`last-requested-mass` field is useful for visual inspection because
`requested-mass` is cleared after `voxel ablate` consumes it.

`surface dump off` clears surface dumps that were already defined. Regression
wrappers can use it after including a visual example input.

Inside DSMC, `surface write-vtp` writes a one-shot VTP snapshot immediately.
This mirrors `voxel write-vtu`: DSMC owns the run loop, and the input script can
write the current ISTHMUS surface after each `isthmus surface` regeneration. If
the path does not contain `*`, the current core step number is inserted before
the file extension.

The optional `fix <fix-id> fields <name1> ...` form reads per-surface values
from a DSMC `fix ave/surf` and appends them as VTP triangle cell fields. The
number of field names must match the number of per-surface columns exposed by
the fix. This is the preferred visualization path for coupled DSMC runs because
the ISTHMUS triangle geometry and DSMC surface quantities are written into the
same VTP file.

## Current Limits

- Only VTP surface dumps are implemented.
- Constant source flux and single-species kinetic-theory flux are implemented.
- DSMC `fix ave/surf` flux ingestion currently supports one MPI rank.
- `select voxels` currently distinguishes ownership presence for the current
  voxel model, not separate voxel groups.
