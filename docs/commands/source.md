# `source` Command

The `source` command defines mass-loss data. It is available in standalone
inputs and in DSMC-hosted inputs through the bridge.

## Syntax

```text
source <id> constant <value> units <units>
```

## Example

```text
source q1 constant 1.8 units kg/m2/s
```

## Description

The current implementation supports a constant mass flux source. The value is
interpreted as mass flux in `kg/m2/s` for the local slab ablation case.

In DSMC-hosted inputs, `source` is mainly useful for no-gas verification cases,
debug probes, and prescribed ablation studies. Coupled DSMC chemistry cases
usually use `surface flux dsmc/reaction` or `surface flux dsmc/surf` instead.

## Current Limits

- Only `constant` sources are implemented.
- Units are recorded but not converted.

## Planned Extensions

- Expressions.
- File or table sources.
- Surface-triangle sources.
