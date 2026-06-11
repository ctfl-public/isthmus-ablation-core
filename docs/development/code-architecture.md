# Code Architecture

This page is a short map for developers reading the code for the first time.

## Core Library

The standalone and DSMC-hosted paths both call the same core library.

| Path | Purpose |
| --- | --- |
| `include/isthmus_ablation/types.hpp` | Plain data structures for input configuration, voxel state, surfaces, history, and checks. |
| `include/isthmus_ablation/model.hpp` | Public `iac::Model` API used by the standalone executable and DSMC bridge. |
| `src/model.cpp` | Voxel geometry, material accounting, mass removal, ISTHMUS calls, dumps, diagnostics, and verification. |
| `src/parser.cpp` | Standalone input parser. It accepts the same IAC command names used by the DSMC bridge. |
| `src/expression.cpp` | Small expression evaluator for exact solutions and verification criteria. |
| `src/main.cpp` | Standalone `ia-core` executable and convergence/report orchestration. |

## DSMC Bridge

The DSMC bridge lives in `dsmc-bridge/`. The build creates a private DSMC
overlay in `build-dsmc/dsmc-overlay/src` and symlinks these files there. The
user's DSMC checkout is not edited.

| Path | Purpose |
| --- | --- |
| `dsmc-bridge/iacbridge.*` | Shared bridge state: one rank-zero core model, config, diagnostics, stats output, and MPI broadcasts. |
| `dsmc-bridge/voxel.*` | `voxel_*` command wrappers. |
| `dsmc-bridge/isthmus.*` | `isthmus_surface` command wrapper and surface broadcast. |
| `dsmc-bridge/surface.*` | `surf_*` wrappers for surface install, flux ingestion, flux measurement, and VTP output. |
| `dsmc-bridge/source.*` | `source` command wrapper. |
| `dsmc-bridge/iac.*` | `iac_*` wrappers for solid timestep, run, stats, verification, and loop variables. |

## Execution Model

Standalone execution parses the whole input into a `Config`, constructs a
`Model`, then executes the command program inside `Model::run`.

DSMC execution is command-by-command. DSMC parses the input and calls bridge
classes directly. Those classes update `IACBridge::config()`, create or mutate
the rank-zero `Model`, and call the same core methods used by standalone mode.

The important boundary is:

```text
DSMC owns gas time:      run, timestep, particles, collisions, surface tallies
IAC owns solid time:     iac_timestep, iac_limit, voxel_ablate, iac_run
ISTHMUS owns meshing:    isthmus_surface through the core surface adapter
```

## Adding A Command

Prefer adding behavior in this order:

1. Add or extend a plain data structure in `types.hpp`.
2. Implement the core behavior in `Model`.
3. Add standalone parsing in `src/parser.cpp`.
4. Add the DSMC bridge wrapper only if the command must be callable inside DSMC.
5. Add or update one example and one focused test.
6. Update command docs and rebuild the manual PDF.

Commands should stay explicit. Avoid hidden advancement: commands that apply
mass loss should not also advance time unless their name says so. Use `iac_run`
for solid advancement.
