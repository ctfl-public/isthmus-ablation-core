# Loop Commands

The standalone runner supports a small DSMC/SPARTA-style loop subset.

## Syntax

```text
variable <name> loop <N>
label <name>
next <name>
jump SELF <label>
jump SELF <label> until time <target>
limit time <target>
```

## Example

```text
variable ablation_time equal 4.0e-3
label ablate-loop
limit time ${ablation_time}
voxel_ablate solid source q1 policy local face xlo delete yes
run 1
jump SELF ablate-loop until time ${ablation_time}
```

## Description

`variable <name> loop <N>` creates a loop counter.

`label <name>` marks a target location.

`next <name>` advances the loop counter. When the counter is exhausted, the next
`jump` command is skipped.

`jump SELF <label>` jumps to a label in the current input.

`jump SELF <label> until time <target>` jumps only while the standalone
ablation clock is below the target time. This is the preferred pattern for
examples because the input states the physical ablation time directly.

`limit time <target>` clips the current timestep if the next `run` command
would advance past the target. Put it inside the loop before `voxel_ablate` so
the final mass-loss update and the recorded time end exactly at the target.

The current implementation supports only these compact loop patterns. It is
meant to make standalone inputs resemble DSMC/SPARTA coupled scripts without
reimplementing the full DSMC/SPARTA variable language.
