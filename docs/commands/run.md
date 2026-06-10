# `run` Command

The `run` command starts standalone time advancement.

## Syntax

```text
run <steps>
run duration <time>
```

## Examples

```text
run 8
run duration 0.02
```

## Description

`run <steps>` advances a fixed number of steps.

`run duration <time>` advances for a requested physical duration using the
current timestep. If the final step would overshoot, the model clips that step
so the recorded time ends exactly at the requested duration.

For explicit ablation loops, use `limit time <target>` before `voxel_ablate`
and `jump SELF <label> until time <target>` after `run 1`.
