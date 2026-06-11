# `surf_*` Commands

The `surf_*` command family works with triangle surfaces generated from
voxels. These commands are deliberately separate from `voxel` commands because
future DSMC-coupled runs will need to refresh surfaces, apply DSMC surface
tallies, and then map that information back to voxels inside input-file loops.

## Syntax

```text
surf_flux <surface-id> source <source-id> select all
surf_flux <surface-id> source <source-id> select normal nx <x> ny <y> nz <z> [min-cos <c>]
surf_flux <surface-id> source <source-id> select voxels voxels <model>
surf_flux <surface-id> kinetic/theory pressure <p> temperature <T> \
  mole-fraction <x> molecular-mass <kg> reaction-prob <alpha> \
  solid-mass-per-hit <kg> select <selector>
surf_flux <surface-id> dsmc/surf fix <fix-id> quantity incident-number-flux \
  [reaction <path> | solid-mass-per-hit <kg>] \
  [reaction-prob <p>] [ablation-dt <dt> | mass-courant <C>]
surf_flux <surface-id> dsmc/surf fix <fix-id> column <N> \
  [reaction <path> | solid-mass-per-hit <kg>] \
  [reaction-prob <p>] [ablation-dt <dt> | mass-courant <C>]
surf_flux <surface-id> dsmc/reaction fix <fix-id> column <N> \
  sample-steps <Nstep> reaction <path> \
  [ablation-dt <dt> | mass-courant <C>] [time-scale <S>]
surf_measure_flux <surface-id> dsmc/reaction fix <fix-id> column <N> \
  sample-steps <Nstep> expected kinetic/theory number-density <n> \
  mole-fraction <x> temperature <T> molecular-mass <kg> \
  [reaction <path>]

surf_dump <dump-id> <surface-id> vtp <N> <path>
surf_dump off

# DSMC bridge only:
surf_write_vtp <path> <surface-id> [fix <fix-id> fields <name1> ...]
```

## Examples

```text
surf_flux skin source q1 select all
surf_flux skin source q1 select normal nx 1.0 ny 0.0 nz 0.0 min-cos 0.5
surf_flux skin source q1 select voxels voxels solid
surf_flux skin kinetic/theory pressure 50.0 temperature 5000.0 \
  mole-fraction 0.21 molecular-mass 5.313e-26 reaction-prob 1.0 \
  solid-mass-per-hit 1.3011869411625376e-23 select all
surf_flux skin dsmc/surf fix sflux mass-courant 0.25
surf_flux skin dsmc/reaction fix rco column 1 sample-steps 20 \
  reaction carbon-co.surf time-scale 1500
surf_measure_flux skin dsmc/reaction fix rco column 1 sample-steps 1 \
  expected kinetic/theory number-density 7.244e23 mole-fraction 0.21 \
  temperature 5000.0 molecular-mass 5.31352e-26 reaction carbon-co.surf

surf_dump skin skin vtp 10 output/sphere/surface_*.vtp
surf_dump off

surf_write_vtp skin output/surface-*.vtp \
  fix sqty fields collision-count incident-number-flux pressure
```

## Description

`surf_flux` assigns one timestep of mass loss to selected triangles on an
existing surface. Source and kinetic-theory modes require an explicit
`select all`, `select normal`, or `select voxels` clause. With `source`, the
source is a constant mass flux in `kg/m2/s`. The requested mass on each
selected triangle is:

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
instance. The preferred pattern is to average only the incident number flux:

```text
compute sflux surf all gas nflux_incident
fix sflux ave/surf all 1 100 100 c_sflux[*] ave one
```

Then either select it by name with `quantity incident-number-flux`, or omit the
quantity because it is the default for `dsmc/surf`. The bridge converts the
selected number flux to solid mass flux with:

```text
flux = number-flux * reaction-probability * solid-mass-per-hit
```

If neither `reaction` nor `solid-mass-per-hit` is supplied, IAC infers one
solid formula unit per incident hit from the current voxel material. For a
material defined as `formula C molar-mass 0.0120107`, this compact default
consumes one carbon atom per counted incident O2 molecule. It is useful for
kinetic-theory collision-flux examples where DSMC is not actually running
surface chemistry.

When `reaction <path>` is used, IAC reads the same SPARTA `surf_react prob`
file used by DSMC. For a file such as:

```text
O2 --> CO + CO
D S 1.0
```

and a material defined as `formula C molar-mass 0.0120107`, IAC infers a
reaction probability of `1.0` and a solid mass per hit equal to two carbon
atoms. The raw `solid-mass-per-hit` and `reaction-prob` arguments are retained
as overrides for cases whose solid consumption cannot be inferred from the
material formula or gas-side reaction formula.

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
probability, species conversion, and reaction tallies. Prefer `reaction <path>`
so the solid mass per reaction is inferred from the same reaction file; use
`solid-mass-per-reaction` only as an explicit override.

`surf_measure_flux` reads the same kind of DSMC reaction count data but does
not apply mass loss. It sums the sampled reactions over the installed surface,
converts them to a number flux with:

```text
reaction-flux = sum(reaction-count) * fnum / (surface-area * DSMC-timestep)
```

and stores scalar diagnostics that can be checked later with `iac_verify`.
The `expected kinetic/theory` arguments compute the ideal-gas one-way
impingement flux for a reactive species:

```text
expected-reaction-flux =
  reaction-prob * mole-fraction * number-density
  * sqrt(kB*temperature/(2*pi*molecular-mass))
```

The command also stores `reaction-count-per-step` and
`expected-reaction-count-per-step`, which are often the clearest DSMC sampling
diagnostics. If `reaction` or `solid-mass-per-reaction` is supplied, it
additionally stores `reaction-mass-flux` and `expected-reaction-mass-flux` in
kg/m2/s by multiplying the number flux by the inferred or supplied solid mass
consumed per reaction.

This command is the first DSMC-hosted regression hook for the coupled path: it
tests geometry generation, surface installation, DSMC surface chemistry
tallies, core diagnostics, and input-file verification without introducing
voxel deletion or remeshing.

DSMC-coupled sources assign triangle mass loss and set the current IAC
ablation timestep; they do not themselves advance the solid clock. Use
`voxel_ablate` to map the pending triangle mass to voxels, then `iac_run 1` to
record the solid step, history, dumps, and stats. By default, the bridge uses
the elapsed DSMC time since the previous coupling update as the ablation time.
The optional `ablation-dt` argument overrides that physical ablation time. For
`dsmc/reaction`, `time-scale`
multiplies both the sampled reaction-count mass and the ablation time. This is
useful for quasi-steady chemistry probes: the input can sample a short DSMC
window and advance the solid over a longer reservoir-equivalent ablation time.
When comparing multiple DSMC sampling lengths at the same ablation time, choose
`time-scale = ablation-update-time / (sample-steps * timestep)`.

The optional `mass-courant` argument chooses the ablation timestep from the
largest current local voxel-face flux. For DSMC surface fluxes, the limiting
flux is the largest positive triangle mass flux that maps to an active,
non-fixed voxel:

```text
dt = C * density * voxel-size / max(triangle-flux)
```

This is the same nondimensional definition as `iac_timestep
mass/courant`: it is the mass that the local flux would remove through one
voxel face during the timestep divided by one voxel mass. Thus
`mass-courant 0.1666666667` is the conservative value `1/6`; even if all six
faces of a voxel saw that same limiting flux, the update cannot remove more
than one voxel mass before carryover/deletion handles the remainder.
`ablation-dt` and `mass-courant` are mutually exclusive.

The mass is stored on the triangle until a later command consumes it:

```text
voxel_ablate solid surface skin policy local delete yes
```

Selectors:

- `select all` applies flux to every triangle.
- `select normal` applies flux only when the triangle normal has
  `dot(normal, direction) >= min-cos`.
- `select voxels` applies flux to triangles that have ISTHMUS ownership
  entries for the named voxel model. This is a forward-compatible hook for
  voxel groups and material subsets.

`surf_dump` writes VTP triangle files on scheduled run steps. The VTP cell
data includes `area`, `requested-mass`, and `last-requested-mass`. The
`last-requested-mass` field is useful for visual inspection because
`requested-mass` is cleared after `voxel_ablate` consumes it.

Inside DSMC, scheduled `surf_dump` output is written when bridge commands
advance the core step. Define the dump before the first surface generation so
the active core model is initialized with the desired schedule.

`surf_dump off` clears surface dumps that were already defined. Regression
wrappers can use it after including a visual example input.

Inside DSMC, `surf_write_vtp` writes a one-shot VTP snapshot immediately.
This mirrors `voxel_write_vtu`: DSMC owns the run loop, and the input script can
write the current ISTHMUS surface after each `isthmus_surface` regeneration. If
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
