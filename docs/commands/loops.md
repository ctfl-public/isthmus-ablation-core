# Loop Commands

The standalone runner supports a small DSMC/SPARTA-style loop subset.

## Syntax

```text
variable <name> loop <N>
variable <name> internal <value>
label <name>
next <name>
jump SELF <label>
iac_limit time <target>
iac_continue time <target> variable <name>
if "${name} > 0" then "jump SELF <label>"
```

## Example

```text
variable ablation_time equal 4.0e-3
variable keep internal 1
label ablate-loop
iac_limit time ${ablation_time}
voxel_ablate solid source q1 policy local face xlo delete yes
iac_run 1
iac_continue time ${ablation_time} variable keep
if "${keep} > 0" then "jump SELF ablate-loop"
```

## Description

`variable <name> loop <N>` creates a loop counter.

`label <name>` marks a target location.

`next <name>` advances the loop counter. When the counter is exhausted, the next
`jump` command is skipped.

`jump SELF <label>` jumps to a label in the current input.

`iac_limit time <target>` clips the current timestep if the next `iac_run`
command would advance past the target. Put it inside the loop before
`voxel_ablate` so the final mass-loss update and the recorded time end exactly
at the target.

The current implementation supports only these compact loop patterns. It is
meant to make standalone inputs resemble DSMC/SPARTA coupled scripts without
reimplementing the full DSMC/SPARTA variable language.
