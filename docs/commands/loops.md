# Loop Commands

The standalone runner supports a small DSMC/SPARTA-style loop subset.

## Syntax

```text
variable <name> loop <N>
label <name>
next <name>
jump SELF <label>
```

## Example

```text
variable i loop 8
label ablate-loop
voxel ablate solid source q1 policy local delete yes
run 1
next i
jump SELF ablate-loop
```

## Description

`variable <name> loop <N>` creates a loop counter.

`label <name>` marks a target location.

`next <name>` advances the loop counter. When the counter is exhausted, the next
`jump` command is skipped.

`jump SELF <label>` jumps to a label in the current input.

The current implementation supports only this compact loop pattern. It is meant
to make standalone inputs resemble future DSMC/SPARTA coupled scripts without
reimplementing the full DSMC/SPARTA variable language.
