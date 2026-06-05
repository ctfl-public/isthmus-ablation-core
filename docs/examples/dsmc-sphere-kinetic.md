# DSMC Sphere Kinetic Example

This example runs from this repository but uses the DSMC executable built in
`/Users/tstoffel1/dsmc/src`. It is a smoke test for the embedded bridge:
DSMC owns the gas domain, native `run` commands, particles, surface reactions,
surface averaging, loops, `remove_surf`, and `surf_modify`; this core owns voxel
state, ISTHMUS surface generation, DSMC-to-triangle reaction-count conversion,
normal-carryover ablation, and regenerated surface geometry.

Run the chemistry case directly with:

```bash
cd examples/dsmc-sphere-kinetic
/Users/tstoffel1/dsmc/src/spa_mac_mpi \
  -screen none -log output/log.sparta -in in.dsmc-sphere-kinetic
```

To enable this case in CTest, configure with the DSMC executable:

```bash
cmake -S . -B build-dsmc \
  -DIAC_DSMC_EXECUTABLE=/Users/tstoffel1/dsmc/src/spa_mac_mpi
cmake --build build-dsmc
ctest --test-dir build-dsmc --output-on-failure
```

The normal standalone build does not include DSMC tests. A DSMC-enabled build
runs the standalone tests plus the coupled DSMC chemistry test.

## Chemistry Loop

The case uses a stationary initial O2/N2 gas with CO enabled as a product
species. SPARTA handles the surface reaction:

```text
species             air.species O2 N2 CO
mixture             air O2 frac 0.21
mixture             air N2 frac 0.79
mixture             air CO frac 0.0
surf_react          ox prob carbon-co.surf
surf_modify         all collide 1 react ox
compute             rco react/surf all ox
fix                 rco ave/surf all 1 20 20 c_rco[*] ave one
```

The local `carbon-co.surf` file represents carbon monoxide formation as:

```text
O2 --> CO + CO
D S 1.0
```

After each DSMC run segment, the bridge maps the averaged surface reaction
counts to ISTHMUS triangles and removes two carbon atoms of solid mass per O2
reaction:

```text
surface flux skin dsmc/reaction fix rco column 1 sample-steps 20 \
  solid-mass-per-reaction 3.98894696e-26 time-scale 1500
voxel ablate solid surface skin policy carryover/normal delete yes
```

Then native DSMC commands clear the old collision surface and install the
regenerated ISTHMUS surface directly from memory:

```text
remove_surf all
isthmus surface skin voxels solid buffer 1 weighting no map yes
surface install skin particle none type 1
surf_modify all collide 1 react ox
```

## Analytical Comparison

The report compares the voxel history to a continuum shrinking-sphere solution
for a uniform stationary O2 reservoir:

```text
Gamma-O2 = n-O2 * sqrt(kB*T/(2*pi*m-O2))
j = Gamma-O2 * reaction-probability * solid-mass-per-reaction
R(t) = R0 - j*t/rho-solid
mass-fraction = (R/R0)^3
```

That comparison is useful, but it has an important caveat. The current DSMC
case is a closed periodic box. Real SPARTA chemistry consumes O2 and creates CO,
so longer DSMC sampling windows can change the local gas composition before the
next ablation update. The convergence report is therefore a coupled
chemistry-sampling diagnostic, not a perfect monotone convergence proof for an
infinite gas reservoir.

Build the PDF/CSV report with:

```bash
cmake --build build-dsmc --target dsmc-convergence-report
```

or run the generator directly:

```bash
python3 tools/run-dsmc-kinetic-convergence.py \
  --dsmc /Users/tstoffel1/dsmc/src/spa_mac_mpi \
  --steps 5,20,80 \
  --loops 6 \
  --resolution 10 \
  --ablation-update-time 0.003 \
  --fnum 2.0e12 \
  --tolerance-percent 20 \
  --pdf
```

The runner keeps the ablation time per coupled update fixed. For each DSMC
sampling length, it sets:

```text
time-scale = ablation-update-time / (sample-steps * timestep)
```

so all cases advance the same solid ablation time while sampling the chemistry
for different numbers of DSMC steps. The generated report plots remaining mass
fraction and equivalent radius recession against the analytical curve.

For a deliberately deep exploratory probe, run:

```bash
cmake --build build-dsmc --target dsmc-half-mass-report
```

This target uses a coarser 5-voxel sphere and a large scaled ablation interval.
It is intended for stress-testing the coupled plumbing, not for tight
verification.

## Visualization Case

The repository also includes a DSMC visualization input:

```bash
cd examples/dsmc-sphere-visual
/Users/tstoffel1/dsmc/src/spa_mac_mpi \
  -screen none -log output/log.sparta -in in.dsmc-sphere-visual
python3 ../../tools/convert-dsmc-visual.py .
```

or, from a DSMC-enabled CMake build:

```bash
cmake --build build-dsmc --target dsmc-visual-case
```

This case writes SPARTA-native fluid text dumps, core voxel/surface VTK files,
and converted ParaView-ready gas files:

```text
examples/dsmc-sphere-visual/output/grid.*.dump
examples/dsmc-sphere-visual/output/fluid-*.vtu
examples/dsmc-sphere-visual/output/voxels-*.vtu
examples/dsmc-sphere-visual/output/surface-*.vtp
examples/dsmc-sphere-visual/output/history.csv
```

Open `fluid-*.vtu`, `voxels-*.vtu`, and `surface-*.vtp` directly in ParaView.
The `.dump` files are native SPARTA text dumps retained as converter inputs and
diagnostics.

The grid dump uses:

```text
compute gas thermal/grid all air temp press
compute frac grid all species massfrac
fix gasave ave/grid all 1 20 20 c_gas[*] c_frac[*] ave one
dump dgrid grid all 20 output/grid.*.dump id xc yc zc vol f_gasave[*]
```

The converted fluid VTU files include temperature, pressure, and O2/N2/CO mass
fractions. The surface VTP snapshots use:

```text
compute rco react/surf all ox
compute sqty surf all air n nflux_incident press
fix sqty ave/surf all 1 20 20 c_sqty[*] c_rco[*] ave one
surface write-vtp skin output/surface-*.vtp \
  fix sqty fields collision-count incident-number-flux pressure co-reaction-count
```

Those VTP files include the current ISTHMUS triangle surface, area and
mass-request fields, DSMC collision count, incident number flux, surface
pressure, and the averaged CO-forming reaction count used to remove carbon.
The voxel VTU files write active voxels with `mass-fraction` as the active cell
scalar.

Reaction heat release is not yet coupled into the gas. The standard diffuse
wall collision model still sets product velocities from the 300 K wall
temperature, so O2 depletion and CO production are physically represented by
species conversion, but an exothermic near-surface temperature rise is not yet
part of this bridge.
