# DSMC Sphere Kinetic Example

This example runs from this repository using the private DSMC/IAC executable
built by the overlay target. It is the simplest coupled DSMC recession case:
SPARTA owns particles, surface collisions, `compute surf`, `fix ave/surf`,
loops, `remove_surf`, and `surf_modify`; IAC owns voxel state, ISTHMUS surface
generation, collision-flux-to-triangle conversion, normal-carryover ablation,
and regenerated surface geometry.

Run it directly with:

```bash
cmake --preset dsmc
cmake --build --preset dsmc

cd examples/dsmc-sphere-kinetic
../../build-dsmc/bin/dsmc-iac \
  -screen none -log output/log.sparta -in in.dsmc-sphere-kinetic
```

To run the DSMC-enabled CTest suite:

```bash
ctest --preset dsmc
```

The normal standalone build does not include DSMC tests. A DSMC-enabled build
runs the standalone tests plus DSMC-hosted bridge tests, including the
`dsmc-sphere-kinetic-grid-convergence` check.

## Collision-Flux Loop

The case uses pure O2 gas in a periodic 3 mm cube. Gas chemistry and gas-gas
collisions are disabled on purpose. This makes the comparison a direct test of
ideal-gas thermal impingement, DSMC surface-collision tallying, triangle-to-
voxel mapping, normal carryover, and repeated ISTHMUS remeshing.

The gas and wall are both 5000 K:

```text
boundary            p p p
species             air.species O2
mixture             gas O2 frac 1.0
mixture             gas temp 5000.0 vstream 0.0 0.0 0.0
surf_collide        1 diffuse 5000.0 1.0
```

SPARTA samples the incident number flux on each surface triangle:

```text
compute             sflux surf all gas nflux_incident
fix                 sflux ave/surf all 1 100 100 c_sflux[*] ave one
run                 100 post no
```

IAC reads the incident number flux and converts it to carbon mass flux:

```text
surf_flux skin dsmc/surf fix sflux mass-courant 0.1666666667
iac limit time ${ablation_time}
voxel_ablate solid surface skin policy carryover/normal delete yes
```

Because chemistry is intentionally disabled here, the command uses the compact
`dsmc/surf` default: the incident flux is treated as a perfectly reactive
collision flux and one carbon formula unit is consumed per counted O2 hit. With
`voxel_material carbon ... molar-mass 0.0120107 formula C`, that means one
carbon atom per hit. This is deliberately simpler than the reacting
`surf_react` path; if the physical interpretation should be `O2 -> CO + CO`,
the analytical comparison differs by a factor of two in solid mass consumed per
incident O2 molecule. The DSMC-measured incident flux is still applied
triangle-by-triangle, then mapped back to voxels through the ISTHMUS ownership
fractions.

After each ablation update, native DSMC commands clear the old collision
surface and install the regenerated ISTHMUS surface directly from memory:

```text
remove_surf all
isthmus_surf skin voxels solid buffer 1 weighting no map yes
surf_install skin particle none type 1
surf_modify all collide 1
```

The committed example uses a 10-voxel sphere, `C_m = 1/6`, and a named
`ablation_time` target. `iac limit time` clips the last solid timestep so the
history ends at exactly that physical ablation time while exercising the full
DSMC-to-ISTHMUS-to-voxel loop. It is a workflow example; the grid-refinement
report below is the stronger accuracy check.

## Analytical Comparison

For pure O2, the one-way thermal impingement flux is:

```text
Gamma = n * sqrt(kB*T/(2*pi*m-O2))
j = Gamma * reaction-probability * solid-mass-per-hit
R(t) = R0 - j*t/rho-solid
mass-fraction = (R/R0)^3
```

The example writes:

```text
examples/dsmc-sphere-kinetic/output/history.csv
```

Check its final mass fraction against the continuum solution with:

```bash
python3 ../../tools/check-dsmc-kinetic.py output/history.csv \
  --number-density 7.244e23 \
  --temperature 5000 \
  --molecular-mass 5.31352e-26 \
  --solid-density 1800 \
  --solid-molar-mass 0.0120107 \
  --solid-atoms-per-hit 1 \
  --initial-radius 5e-4
```

## Grid-Refinement Report

The DSMC CTest suite runs this grid-refinement case without PDF generation.
Build the visual DSMC grid-refinement report with:

```bash
cmake --build --preset dsmc --target dsmc-convergence-report
```

or run the generator directly:

```bash
python3 tools/run-dsmc-kinetic-convergence.py \
  --dsmc build-dsmc/bin/dsmc-iac \
  --resolutions 4,8,12 \
  --target-mass-fraction 0.2 \
  --sample-steps 10 \
  --mass-courant 0.1666666667 \
  --domain-half-width 6.5e-4 \
  --grid-cells 6 \
  --fnum 2.0e11 \
  --tolerance-percent 25 \
  --require-improvement \
  --pdf
```

The runner generates temporary input files under the build directory, computes
the physical ablation time corresponding to the requested target mass fraction,
and lets the runtime `mass-courant` command choose each solid ablation timestep
from the current maximum triangle mass flux. The final update is clipped so
each case ends at the same analytical ablation time. The pass/fail check uses
the finest-grid mass fraction, voxelized volume fraction, and radius errors;
`--require-improvement` also requires the finest mass error to improve over the
coarsest case.

```text
build-dsmc/output/dsmc-sphere-kinetic-grid-convergence/summary.csv
build-dsmc/output/dsmc-sphere-kinetic-grid-convergence/report.pdf
```

The report is optimized as a quick regression-style coupled check. It uses a
small periodic box around the sphere and a modest `4,8,12` voxel-resolution
ladder so the run remains short while still showing grid refinement toward the
continuum sphere solution.

## Chemistry Note

The repository still includes a one-step DSMC chemistry flux verification in:

```text
tests/inputs/dsmc-sphere-flux/in.dsmc-sphere-flux.verify
```

That test keeps the `dsmc/reaction` bridge path covered. The main coupled
sphere recession example intentionally uses collision flux instead, because it
isolates the kinetic-theory comparison from chemistry, composition depletion,
and heat-release modeling.
