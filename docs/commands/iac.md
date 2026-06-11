# `iac_*` Commands

The `iac_*` command family controls IAC state that is not naturally owned by a
single voxel, ISTHMUS, or surface command. Core commands use the same names in
standalone mode and in the DSMC bridge wherever the host has the needed
capability.

For DSMC-hosted inputs, write long bridge commands on one physical line. The
examples below are wrapped only for readability.

## Syntax

```text
iac_timestep <dt>
iac_timestep mass/courant <C> source <source-id>
iac_stats <N>
iac_stats_style <column> <column> ...
iac_limit time <target>
iac_continue time <target> variable <name>
iac_set <name> time
iac_set <name> step
iac_set <name> dt
iac_set <name> diagnostic <diagnostic-name>
iac_verify <quantity> exact <expression> tolerance <value> [percent|absolute]
```

## Examples

```text
surf_measure_flux skin dsmc/reaction fix rco column 1 sample-steps 1 \
  expected kinetic/theory number-density 7.244e23 mole-fraction 0.21 \
  temperature 5000.0 molecular-mass 5.31352e-26 reaction carbon-co.surf

iac_verify area exact area-exact tolerance 2.0 percent
iac_verify nreact exact nreact-exact tolerance 5.0 percent
iac_verify rmflux exact rmflux-exact tolerance 5.0 percent
iac_set fluxerr diagnostic rflux-errpct
print "reaction flux error = ${fluxerr} percent"

source q1 constant 1.8 units kg/m2/s
iac_timestep mass/courant 0.5 source q1
iac_stats 1
iac_stats_style step time nvox ndel mass mf front

variable keep internal 1
label ablate-loop
iac_limit time 1.1e-3
voxel_ablate solid source q1 policy local face xlo delete yes
iac_run 1
iac_continue time 1.1e-3 variable keep
if "${keep} > 0" then "jump SELF ablate-loop"
```

## Description

`iac_timestep` controls the solid ablation timestep used by this package. In
DSMC-hosted inputs, it does not change DSMC's native fluid timestep. Use native
DSMC `timestep` for particle motion and collisions; use `iac_timestep` for
voxel mass-loss updates.

The explicit form sets the ablation timestep directly. The `mass/courant` form
computes:

```text
dt = C * density * voxel-size / source
```

for the named constant source.

`iac_run` advances IAC solid time/history/dumps/stats. It is intentionally
separate from native DSMC `run`, which advances particles and gas collisions.
Use `voxel_ablate` or `surf_flux` to apply mass changes, then `iac_run 1` to
record the solid step.

`iac_stats` and `iac_stats_style` print the core's own ablation table. In
DSMC-hosted inputs, they intentionally do not modify DSMC's native `stats` or
`stats_style` settings. The title block and table header are printed the first
time `iac_run` advances the core step; later rows are printed at the requested
IAC interval.

`iac_limit time <target>` clips the current IAC solid timestep so the next
voxel mass update cannot advance past the requested physical ablation time.
This is usually placed after a command such as `surf_flux ... mass-courant`,
which may have just recomputed the timestep from the current flux, and before
`voxel_ablate`.

`iac_continue time <target> variable <name>` writes `1` to a DSMC internal
variable while the IAC solid time is less than the target and `0` once it has
reached or exceeded the target. In standalone mode, the same pattern is parsed
as a compact time-limited jump. Use it with `iac_limit time`, DSMC-style `if`,
and `jump` commands to run an ablation loop to an exact physical solid time:

```text
variable keep internal 1
label coupled-loop
run 100 post no
surf_flux skin dsmc/surf fix sflux mass-courant 0.1666666667
iac_limit time 2.5e-2
voxel_ablate solid surface skin policy carryover/normal delete yes
iac_run 1
iac_continue time 2.5e-2 variable keep
if "${keep} > 0" then "jump SELF coupled-loop"
```

If the named variable does not exist, the DSMC bridge creates it as an internal
variable. If it does exist, it must already be internal style. For portable
standalone/DSMC inputs, declare it explicitly with `variable keep internal 1`.

`iac_set` copies IAC state into a DSMC internal variable. Supported quantities
are `time`, `step`, `dt`, and `diagnostic <diagnostic-name>`. This is useful
for printing, conditional input logic, and lightweight debugging:

```text
iac_set solidtime time
print "IAC solid time = ${solidtime}"
```

`iac_verify` compares a named diagnostic to an exact expression using the same
tolerance modes as the standalone `iac_verify` command. The exact expression can
refer to diagnostics registered by earlier bridge commands. For example,
`surf_measure_flux` registers:

- `area`
- `area-exact`
- `area-errpct`
- `nreact`
- `nreact-exact`
- `rflux`
- `rflux-exact`
- `rflux-ratio`
- `rflux-errpct`
- `rmflux`, when `reaction` or `solid-mass-per-reaction` is provided
- `rmflux-exact`, when `reaction` or `solid-mass-per-reaction` is provided

The DSMC flux verification in `tests/inputs/dsmc-sphere-flux` is deliberately
an instantaneous one-step test. It checks the initial kinetic-theory O2
reaction count and equivalent solid mass flux before the perfectly consuming
surface depletes the nearby O2 population. Longer depletion and coupled
ablation cases belong in examples and report targets, not the default test
suite.

History-based quantities such as `mf`, `front`, and `rad` can be
checked with `norm final`, `norm max`, or `norm rms` once the bridge has
advanced at least one IAC step. Scalar diagnostics registered by bridge
commands are checked directly.

## Current Limits

- `iac_continue` is shared for the common time-loop pattern. `iac_set` is a
  DSMC bridge command because it interacts with DSMC internal variables.
- A full DSMC-hosted `convergence` command is not implemented. Convergence
  suites still belong at the CTest/report layer or in explicit DSMC input loops
  because true convergence orchestration requires rerunning the host input with
  different variable overrides.
