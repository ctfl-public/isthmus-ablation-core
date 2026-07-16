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
  [ablation-dt <dt> | mass-courant <C>] [time-scale <S>] \
  [select all | select normal nx <x> ny <y> nz <z> [min-cos <c>]]
surf_flux <surface-id> dsmc/mass-flux fix <fix-id> quantity mass-flux \
  units <flux|flow> [ablation-dt <dt> | mass-courant <C>] \
  [select normal nx <x> ny <y> nz <z> [min-cos <c>]]
surf_flux <surface-id> dsmc/mass-flux fix <fix-id> column <N> \
  units <flux|flow> [ablation-dt <dt> | mass-courant <C>] \
  [select normal nx <x> ny <y> nz <z> [min-cos <c>]]
surf_measure_flux <surface-id> dsmc/reaction fix <fix-id> column <N> \
  sample-steps <Nstep> expected kinetic/theory number-density <n> \
  mole-fraction <x> temperature <T> molecular-mass <kg> \
  [reaction <path>]
surf_measure_flux <surface-id> dsmc/mass-flux fix <fix-id> quantity mass-flux \
  units <flux|flow> expected kinetic/theory number-density <n> \
  mole-fraction <x> temperature <T> molecular-mass <kg> \
  [reaction-prob <p> solid-mass-per-reaction <kg>]

dsmc_converge flux <surface-id> fix <fix-id> quantity mass-flux every <Nstep> \
  reduce <sum|ave|sum-area|ave-area> rel <tol> cv <tol> window <N> \
  max-iter <N> [min-iter <N>] [passes <N>] [variable <name>] \
  [select all | select normal nx <x> ny <y> nz <z> [min-cos <c>]]
dsmc_converge flux boundary <xlo|xhi|ylo|yhi|zlo|zhi> fix <fix-id> \
  quantity mass-flux every <Nstep> rel <tol> cv <tol> window <N> \
  max-iter <N> [min-iter <N>] [passes <N>] [variable <name>]

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
  reaction carbon-co.surf mass-courant 0.1666666667
surf_flux skin dsmc/reaction fix rco column 1 sample-steps 100 \
  reaction carbon-co.surf mass-courant 1.0 \
  select normal nx 1.0 ny 0.0 nz 0.0 min-cos 0.5
surf_flux skin dsmc/mass-flux fix mflux quantity mass-flux units flux \
  mass-courant 0.3333333333
surf_measure_flux skin dsmc/reaction fix rco column 1 sample-steps 1 \
  expected kinetic/theory number-density 7.244e23 mole-fraction 0.21 \
  temperature 5000.0 molecular-mass 5.31352e-26 reaction carbon-co.surf
surf_measure_flux skin dsmc/mass-flux fix mflux quantity mass-flux units flux \
  expected kinetic/theory number-density 7.244e23 mole-fraction 0.21 \
  temperature 5000.0 molecular-mass 5.31352e-26 \
  solid-mass-per-reaction 3.98894696e-26
dsmc_converge flux skin fix mflux quantity mass-flux every 20 reduce sum-area \
  rel 0.10 cv 0.10 window 3 min-iter 3 max-iter 20
dsmc_converge flux boundary xlo fix bflux quantity mass-flux every 20 \
  rel 0.10 cv 0.10 window 3 min-iter 3 max-iter 20

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
surface element per sampled timestep. The bridge converts it to a real solid
mass rate over the sampled DSMC window:

```text
mass = reaction-count * sample-steps * fnum * solid-mass-per-reaction
mflux = mass / (triangle-area * sample-steps * DSMC-timestep)
```

The solid timestep is then chosen by `ablation-dt`, `mass-courant`, or the
sampled DSMC time when neither is specified. The requested triangle mass is
`mflux * triangle-area * solid-timestep`. This is the preferred path for
chemically reacting DSMC ablation because SPARTA owns the surface reaction
probability, species conversion, and reaction tallies, while IAC owns the solid
timestep and voxel mass ledger. Prefer `reaction <path>` so the solid mass per
reaction is inferred from the same reaction file; use
`solid-mass-per-reaction` only as an explicit override.

`surf_measure_flux` reads the same kind of DSMC reaction count data but does
not apply mass loss. It sums the sampled reactions over the installed surface,
converts them to a number flux with:

```text
rflux = sum(reaction-count) * fnum / (area * DSMC-timestep)
```

and stores scalar diagnostics that can be checked later with `iac_verify`.
The `expected kinetic/theory` arguments compute the ideal-gas one-way
impingement flux for a reactive species:

```text
rflux-exact =
  reaction-prob * mole-fraction * number-density
  * sqrt(kB*temperature/(2*pi*molecular-mass))
```

The command also stores `nreact` and
`nreact-exact`, which are often the clearest DSMC sampling
diagnostics. If `reaction` or `solid-mass-per-reaction` is supplied, it
additionally stores `rmflux` and `rmflux-exact` in
kg/m2/s by multiplying the number flux by the inferred or supplied solid mass
consumed per reaction.

This command is the first DSMC-hosted regression hook for the coupled path: it
tests geometry generation, surface installation, DSMC surface chemistry
tallies, core diagnostics, and input-file verification without introducing
voxel deletion or remeshing.

With `dsmc/mass-flux`, the command reads a mass flux already computed inside
DSMC. This mode is intended for DSMC branches that provide
`compute react/surf/mass/flux`, for example:

```text
compute mflux react/surf/mass/flux all ox mass 0.0240214 norm flux
fix mflux ave/surf all 1 20 20 c_mflux[*] ave one
```

Here DSMC owns the chemistry, reaction counting, normalization by surface area
and timestep, and conversion to solid mass flux. IAC normally reads
`quantity mass-flux` from the `fix ave/surf` result. The lower-level
`column <N>` form remains available when a custom DSMC fix exposes the desired
mass flux in a nonstandard column. IAC optionally filters triangles by normal
direction, maps
the triangle fluxes through the ISTHMUS ownership fractions, chooses the solid
timestep from `mass-courant` when requested, and then `voxel_ablate` consumes
the pending triangle mass. Use `units flux` when the DSMC column is already
`kg/m2/s`; use `units flow` when the DSMC column is `kg/s` per triangle and
IAC should divide by triangle area.

Run DSMC far enough for the averaging fix to publish before calling
`surf_flux`. For example, `fix mflux ave/surf all 5 5000 25000 ...` needs a
25,000-step run after the fix is defined; reading it after only 5,000 steps
will use stale per-surface data and is rejected by the bridge.

`surf_measure_flux ... dsmc/mass-flux` performs the same read and reduction
without changing voxel mass. With `expected kinetic/theory`, it compares the
area-averaged DSMC mass flux to the ideal-gas one-way impingement estimate:

```text
Gamma = mole-fraction * number-density
        * sqrt(kB*temperature/(2*pi*molecular-mass))
rmflux-exact = reaction-prob * solid-mass-per-reaction * Gamma
```

The command registers `area`, `area-exact`, `rmflux`, `rmflux-exact`,
`rmflux-errpct`, and `area-errpct` for later `iac_verify` checks.

`dsmc_converge` is a DSMC-bridge command that runs native DSMC in repeated
blocks before ablation. The command is deliberately bounded: `every` is the
number of DSMC steps per block, and `max-iter` is the maximum number of blocks
allowed before the command errors. After each block it reads the selected
surface column, reduces it with `sum`, `ave`, `sum-area`, or `ave-area`, and
checks both relative change and a rolling coefficient of variation. This gives
two clean coupled workflows:

```text
# Explicit input-file loop.
run 20 post no every 20 "iac_spa_stats"
surf_flux skin dsmc/mass-flux fix mflux quantity mass-flux \
  units flux mass-courant 0.3333333333
voxel_ablate solid surface skin policy carryover/normal delete yes
iac_run 1
iac_spa_stats

# Compact convergence block.
dsmc_converge flux skin fix mflux quantity mass-flux every 20 reduce sum-area \
  rel 0.10 cv 0.10 window 3 max-iter 20
surf_flux skin dsmc/mass-flux fix mflux quantity mass-flux \
  units flux mass-courant 0.3333333333
voxel_ablate solid surface skin policy carryover/normal delete yes
iac_run 1
iac_spa_stats
```

For a flat-plate case that reacts on a DSMC box boundary instead of an
installed surface, average the boundary mass-flux compute as a six-row vector
and select the face in `dsmc_converge`:

```text
bound_modify xlo collide wall react ox
compute bflux react/boundary/mass/flux ox mass 0.0240214
fix bflux ave/time 1 20 20 c_bflux[*][1] mode vector ave one

dsmc_converge flux boundary xlo fix bflux quantity mass-flux every 20 \
  rel 0.10 cv 0.10 window 3 max-iter 20
```

For a DSMC column normalized as `kg/m2/s`, `sum-area` returns total mass loss
rate in `kg/s` over the selected triangles, while `ave-area` returns the
area-averaged mass flux in `kg/m2/s`. Add `select normal ...` when the
convergence diagnostic should follow only one face of a reconstructed surface.
The boundary form reads one face from the averaged six-face boundary vector, so
it does not use `reduce` or surface-normal selection.

The compact command is quiet during normal console output. Use `iac_spa_stats`
after the IAC update to print the compact `[SPA]` gas stats row for the DSMC
block that fed that update. The command also registers diagnostics named
`dsmc-converge-value`, `dsmc-converge-rel`, `dsmc-converge-cv`,
`dsmc-converge-iter`, `dsmc-converge-steps`, and `dsmc-converged`.

DSMC-coupled sources assign triangle mass loss and set the current IAC
ablation timestep; they do not themselves advance the solid clock. Use
`voxel_ablate` to map the pending triangle mass to voxels, then `iac_run 1` to
record the solid step, history, dumps, and stats. By default, the bridge uses
the elapsed DSMC time since the previous coupling update as the ablation time.
The optional `ablation-dt` argument overrides that physical ablation time. For
`dsmc/reaction`, `time-scale` scales the sampled DSMC window before the reaction
rate is formed. In normal coupled runs, prefer `mass-courant` so the current
local surface flux chooses a stable solid timestep.

For `dsmc/reaction`, an optional `select normal` clause restricts which
reaction tallies are mapped back into voxel mass loss. SPARTA can still collide
and react with the full installed surface; this selector only controls the
solid ablation update. This is useful when a full watertight surface is needed
for particles, but only one exposed face should recess in the cutout model.

The optional `mass-courant` argument chooses the ablation timestep from the
largest current local voxel mass-removal rate. For surface fluxes, triangle
mass rates are first mapped to active, non-fixed voxels through the ISTHMUS
ownership fractions. The limiting rate is then converted to an equivalent
voxel-face flux:

```text
equivalent-face-flux = max(mvox-rate) / voxel-size^2
dt = C * mvox / max(mvox-rate)
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
data includes `area`, `mreq`, `mreq-last`, `mflux`,
`mflux-last`, and `selected`. The `last-*` fields are useful for visual
inspection because `mreq` is cleared after `voxel_ablate` consumes
it. `selected = 1` marks triangles that received flux during the most recent
surface-flux command.

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
