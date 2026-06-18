# DSMC Sphere Mass Flux Example

This example uses the direct mass-flux feature from the DSMC `React_to_Flux`
branch. DSMC computes a per-triangle carbon mass flux from surface chemistry,
IAC reads that flux from `fix ave/surf`, maps it through the ISTHMUS ownership
fractions, and then ablates voxels with normal carryover.

This is different from the older kinetic sphere example. The older case reads
incident collision flux and converts it inside IAC. This case lets DSMC own the
surface chemistry and the mass-flux normalization directly.

## Requirements

Build against a DSMC source tree that contains:

```text
src/compute_react_surf_mass_flux.cpp
src/compute_react_surf_mass_flux.h
```

Then build the private DSMC/IAC executable:

```bash
make mac_mpi
```

or, on a typical Linux/MPI system:

```bash
make mpi
```

## Explicit Loop

`examples/dsmc-sphere-mass-flux/in.explicit-loop` shows the most transparent
workflow. The input creates the voxel sphere, installs the ISTHMUS surface into
DSMC, runs DSMC for a fixed number of particle steps, reads the mass-flux
quantity, advances the solid, remeshes, and loops:

```text
fix mflux ave/surf all 1 20 20 c_mflux[*] ave one
run 20 post no every 20 "iac_spa_stats"

surf_flux skin dsmc/mass-flux fix mflux quantity mass-flux \
  units flux mass-courant 0.3333333333
iac_limit time ${ablation_time}
unfix mflux
voxel_ablate solid surface skin policy carryover/normal delete yes
iac_run 1

remove_surf all
isthmus_surface skin voxels solid buffer 1 weighting no map yes
surf_install skin particle none type 1
surf_modify all collide 1 react ox
```

The example sets native `stats` to a large interval and uses `iac_spa_stats`
at the DSMC/solid handoff. That keeps the console in the compact `[SPA]` and
`[IAC]` format instead of printing a full native SPARTA summary after every
short gas block.

Run it from the example folder so the local species, collision, and reaction
files resolve naturally:

```bash
cd examples/dsmc-sphere-mass-flux
../../build-dsmc/bin/dsmc-iac -in in.explicit-loop
```

## Convergence Command

`examples/dsmc-sphere-mass-flux/in.converge-command` uses the same physics but
replaces the fixed DSMC block with `dsmc_converge`:

```text
dsmc_converge flux skin fix mflux quantity mass-flux every 20 reduce sum-area \
  rel 0.25 cv 0.25 window 2 min-iter 2 max-iter 4
```

`every 20` is the number of DSMC steps per convergence block. The command runs
one block at a time, reads `quantity mass-flux`, and stops once the
relative change and rolling coefficient of variation are below the requested
tolerances. `max-iter` is a safety guard; the command errors if the selected
fluid statistic does not settle before that many blocks. The example prints the
IAC convergence and ablation rows first, then calls `iac_spa_stats` after
`iac_run` so the compact `[SPA]` row for the DSMC block appears after the IAC
update that consumed it.

Run it with:

```bash
cd examples/dsmc-sphere-mass-flux
../../build-dsmc/bin/dsmc-iac -in in.converge-command
```

## Verification Tests

DSMC-linked builds require `compute react/surf/mass/flux`, so CTest always adds
three fixed-sphere checks:

```bash
ctest --test-dir build-dsmc -R dsmc-mass-flux --output-on-failure
```

The tests cover:

- `dsmc-mass-flux-explicit-verification`: one DSMC step on a fixed surface,
  comparing area-averaged mass flux to ideal-gas impingement theory.
- `dsmc-converge-verification`: the same fixed-surface check, but
  with the compact `dsmc_converge` command driving the DSMC block.
- `dsmc-mass-flux-ablation-verification`: a tiny chemistry-driven ablation
  smoke test that maps direct DSMC mass flux to voxels and checks final `mf`
  and `vf`.

The analytical fixed-surface comparison uses:

```text
Gamma = x * n * sqrt(kB*T/(2*pi*m))
rmflux = Gamma * solid-mass-per-reaction
```

For the committed tests, `solid-mass-per-reaction` is two carbon atoms because
the surface reaction is `O2 --> CO + CO`.

The compact rough-sample full chemistry/remesh regression is documented with
the pregen TIFF carbon recession example.
