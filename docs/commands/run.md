# `iac_run` Command

The `iac_run` command advances IAC solid time/history/dumps/stats. It is
separate from DSMC/SPARTA's native `run` command, which advances particles and
gas collisions.

## Syntax

```text
iac_run <steps>
iac_run duration <time>
```

## Examples

```text
iac_run 8
iac_run duration 0.02
```

## Description

`iac_run <steps>` advances a fixed number of solid ablation steps.

`iac_run duration <time>` advances for a requested physical duration using the
current timestep. If the final step would overshoot, the model clips that step
so the recorded time ends exactly at the requested duration.

For explicit ablation loops, use `iac_limit time <target>` before
`voxel_ablate`, then use `iac_continue` with DSMC-style `if ... then "jump SELF
<label>"` after `iac_run 1`.
