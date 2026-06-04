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
current timestep. The model chooses enough whole steps to reach or slightly
exceed the requested duration.
