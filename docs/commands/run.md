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

`run duration <time>` advances for a requested physical duration. With the
current mass-Courant timestep, the model chooses an integer step count near the
safe timestep and adjusts `dt` so the final time lands on the requested
duration.

