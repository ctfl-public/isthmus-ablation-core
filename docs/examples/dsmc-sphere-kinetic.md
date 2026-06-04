# DSMC Sphere Kinetic Example

This example runs from this repository but uses the DSMC executable built in
`/Users/tstoffel1/dsmc/src`. It is a smoke test for the embedded bridge:
DSMC owns the gas domain, native `run` commands, particles, surface collision
state, loops, `compute surf`, `fix ave/surf`, `remove_surf`, and `surf_modify`;
this core owns voxel state, ISTHMUS surface generation, DSMC-to-triangle flux
conversion, normal-carryover ablation, and regenerated surface geometry.

Run it with:

```bash
/Users/tstoffel1/dsmc/src/spa_mac_mpi -in examples/dsmc-sphere-kinetic/in.dsmc-sphere-kinetic
```

To enable this case in CTest, configure with the DSMC executable:

```bash
cmake -S . -B build-dsmc \
  -DIAC_DSMC_EXECUTABLE=/Users/tstoffel1/dsmc/src/spa_mac_mpi
cmake --build build-dsmc
ctest --test-dir build-dsmc --output-on-failure
```

The normal standalone build does not include DSMC tests. A DSMC-enabled build
runs the standalone tests plus the coupled DSMC convergence test.

The case uses a stationary, spatially uniform O2/N2 gas. DSMC samples the
incident number flux on every installed ISTHMUS triangle:

```text
compute sflux surf all air nflux_incident
fix sflux ave/surf all 1 5 5 c_sflux[*] ave one
surface flux skin dsmc/surf fix sflux column 1 reaction-prob 1.0 \
  solid-mass-per-hit 1.99447348e-26 mass-courant 0.25
```

The bridge interprets the selected `fix ave/surf` column as number flux and
converts it to solid mass flux with `reaction-prob * solid-mass-per-hit`.

The new DSMC bridge commands used by the example are:

```text
voxel material carbon density 1800.0
voxel create solid sphere diameter 1.0e-3 resolution 20 material carbon
isthmus surface skin voxels solid buffer 1 weighting no map yes
surface install skin particle none type 1
surface flux skin dsmc/surf fix sflux column 1 reaction-prob 1.0 solid-mass-per-hit 1.99447348e-26 mass-courant 0.25
voxel ablate solid surface skin policy carryover/normal delete yes
voxel write-history solid output/dsmc-sphere-kinetic/history.csv
```

After ablation, the input uses native DSMC commands to clear the old surface and
then installs the regenerated ISTHMUS surface directly from memory:

```text
remove_surf all
isthmus surface skin voxels solid buffer 1 weighting no map yes
surface install skin particle check type 1
surf_modify all collide 1
```

`surface install` currently expects existing DSMC surfaces to be removed first.
That keeps the first bridge implementation simple and lets DSMC's own
`remove_surf` command clear grid-surface and particle ownership state.

The history file records the voxel-side response after each coupling interval.
For an analytical comparison in this uniform stationary limit, the expected
continuum recession speed is:

```text
Gamma = x*p/(kB*T) * sqrt(kB*T/(2*pi*m))
j = Gamma * reaction-prob * solid-mass-per-hit
R(t) = R0 - j*t/rho-solid
mass-fraction = (R/R0)^3
```

The helper script below compares the written history against this limit. The
example uses an effective full-air molecular mass because the DSMC compute
samples mixture `air` with `mix-ID` set to `air`.

```bash
python3 tools/check-dsmc-kinetic.py output/dsmc-sphere-kinetic/history.csv \
  --number-density 7.244e23 \
  --temperature 5000.0 \
  --molecular-mass 4.753323e-26 \
  --solid-density 1800.0 \
  --solid-mass-per-hit 1.99447348e-26 \
  --initial-radius 5.0e-4 \
  --tolerance-percent 1.0
```

The convergence runner repeats the DSMC-hosted case with several DSMC step
counts per coupling interval and writes both a CSV and a small PDF report:

The CTest convergence runner uses a fixed ablation timestep instead of a mass
Courant timestep:

```bash
python3 tools/run-dsmc-kinetic-convergence.py \
  --dsmc /Users/tstoffel1/dsmc/src/spa_mac_mpi \
  --steps 5,20,80 \
  --loops 6 \
  --ledger-steps 5 \
  --resolution 10 \
  --ablation-dt 1.5e-4 \
  --fnum 1.0e13 \
  --tolerance-percent 1.0 \
  --require-monotonic \
  --pdf
```

The fixed `ablation-dt` keeps all DSMC sampling lengths on the same physical
ablation-time axis. `--ledger-steps 5` applies five voxel mass-ledger updates
between DSMC/ISTHMUS refreshes, reusing the most recent sampled surface flux.
This keeps the case quick while still using DSMC surface tallies to set the
ablation flux. The report plots both mass fraction and equivalent radius errors
against the continuum analytical solution for each DSMC sampling length. The
history plots show remaining mass fraction and radius recession in microns.

Using a mass Courant number larger than one is not equivalent to this
subcycling. `mass-courant 1.0` advances to the next predicted voxel depletion
event. Larger values intentionally step beyond that event with stale geometry
and stale mapped flux, which can skip deletion/carryover ordering and roughen
late-stage surfaces.

or, from a DSMC-enabled CMake build:

```bash
cmake --build build-dsmc --target dsmc-convergence-report
```

For exploratory deep-recession probes, the runner also accepts:

```bash
python3 tools/run-dsmc-kinetic-convergence.py \
  --dsmc /Users/tstoffel1/dsmc/src/spa_mac_mpi \
  --steps 20 \
  --loops 30 \
  --resolution 20 \
  --mass-courant 1.0 \
  --ledger-steps 1 \
  --bad-edges 20
```

`--resolution` changes the voxel count across the sphere diameter.
`--ledger-steps` repeats voxel mass-ledger updates between DSMC/ISTHMUS surface
refreshes, reusing the last sampled DSMC surface flux. This can reduce coupled
refresh cost, but it is approximate because the DSMC surface is stale during
the inner ledger loop. `--bad-edges` sets DSMC `global nedgebadnum`; it is an
exploratory late-stage tolerance for surfaces with a small number of unmatched
edges. It should not be used as a production fix until the surface cleanup path
is understood.

The runner also accepts `--gas-collisions no`, but that mode is not used for
the validation case. With the current periodic domain and diffuse 300 K surface,
collisionless particles that hit the cold sphere are re-emitted cold and are not
rethermalized. A collisionless quick case will need an explicit gas-bath
reinitialization or thermostat before it can represent the intended uniform
5000 K Maxwellian reservoir.
