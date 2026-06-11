# `include` Command

The `include` command reads another input file at the point where the command
appears. This keeps examples and tests from duplicating the same setup.

## Syntax

```text
include <path>
```

## Examples

```text
include ../../../examples/slab-direct-ablation/in.slab-direct-ablation
```

## Description

Relative paths are resolved from the directory of the file that contains the
`include` command.

The command is useful for regression tests:

```text
include ../../../examples/slab-direct-ablation/in.slab-direct-ablation

iac_verify remaining-mass exact "initial-mass - q1*area*time" tolerance 0.01 percent norm max
iac_verify mass-fraction exact "1.0 - q1*time/(rho*length)" tolerance 0.01 percent norm max
iac_verify front exact "q1*time/rho" tolerance 0.01 percent norm final
```

The example owns the physical case. The test wrapper owns the pass/fail
criteria.
