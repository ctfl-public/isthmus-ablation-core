# `iac` Commands

The `iac` command family is available in the DSMC bridge. It exposes
repository-level diagnostics and checks that are not naturally owned by a
single `voxel`, `isthmus`, or `surface` command.

Standalone inputs should keep using the standalone `verify` command. DSMC
inputs use `iac verify` because DSMC already owns the top-level input parser
and this command avoids collisions with native DSMC syntax.

For DSMC-hosted inputs, write long bridge commands on one physical line. The
examples below are wrapped only for readability.

## Syntax

```text
iac timestep <dt>
iac timestep mass/courant <C> source <source-id>
iac stats <N>
iac stats-style <column> <column> ...
iac verify <quantity> exact <expression> tolerance <value> [percent|absolute]
iac print <quantity>
```

## Examples

```text
surface measure-flux skin dsmc/reaction fix rco column 1 sample-steps 80 \
  expected kinetic/theory number-density 7.244e23 mole-fraction 0.21 \
  temperature 5000.0 molecular-mass 5.31352e-26 reaction-prob 1.0

iac verify surface-area exact expected-surface-area tolerance 2.0 percent
iac verify reaction-flux exact expected-reaction-flux tolerance 65.0 percent
iac print reaction-flux-error-percent

source q1 constant 1.8 units kg/m2/s
iac timestep mass/courant 0.5 source q1
iac stats 1
iac stats-style step time active-voxels deleted-voxels remaining-mass mass-fraction front
```

## Description

`iac timestep` controls the ablation timestep used by this package when DSMC is
the host executable. It does not change DSMC's native fluid timestep. Use native
DSMC `timestep` for particle motion and collisions; use `iac timestep` for
voxel mass-loss updates.

The explicit form sets the ablation timestep directly. The `mass/courant` form
matches the standalone timestep command and computes:

```text
dt = C * density * voxel-size / source
```

for the named constant source.

`iac stats` and `iac stats-style` print the core's own ablation table while
DSMC is the host executable. They intentionally do not modify DSMC's native
`stats` or `stats_style` settings. The title block and table header are printed
the first time a bridge command advances the core step; later rows are printed
at the requested IAC interval.

`iac verify` compares a named diagnostic to an exact expression using the same
tolerance modes as the standalone `verify` command. The exact expression can
refer to diagnostics registered by earlier bridge commands. For example,
`surface measure-flux` registers:

- `surface-area`
- `expected-surface-area`
- `surface-area-error-percent`
- `reaction-count-per-step`
- `reaction-flux`
- `expected-reaction-flux`
- `reaction-flux-ratio`
- `reaction-flux-error-percent`

The current DSMC flux verification is deliberately broad because it documents a
known setup problem: the sampled reaction flux in the current reservoir sphere
case is substantially below the ideal-gas kinetic-theory value. The test still
belongs in the suite because it proves that DSMC can host the core's
diagnostic/verification machinery directly.

`iac print` writes one diagnostic to the DSMC screen and log. It is intended for
small interactive probes and should not replace `iac verify` in regression
tests.

History-based quantities such as `mass-fraction`, `front`, and `radius` can be
checked with `norm final`, `norm max`, or `norm rms` once the bridge has
advanced at least one IAC step. Scalar diagnostics registered by bridge
commands are checked directly.

## Current Limits

- `iac` is currently a DSMC bridge command only.
- A full DSMC-hosted `convergence` command is not implemented. Convergence
  suites still belong at the CTest/report layer or in explicit DSMC input loops
  because true convergence orchestration requires rerunning the host input with
  different variable overrides.
