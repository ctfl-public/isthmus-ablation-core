# Hackathon DSMC/IAC Benchmark

This benchmark is the shared hackathon case for timing coupled DSMC/ISTHMUS-
ablation workflows on a shrinking carbon sphere.

The gas-side DSMC calculation is intentionally only a visualization/statistics
driver.  Recession is prescribed from kinetic theory for pure O2 impingement at
50 Pa and 5000 K, so DSMC does not need to run for the same physical time as
the ablation step.

## Conditions

- Sphere diameter: 1.0e-3 m
- Solid: carbon, 1800 kg/m3
- Gas: pure O2
- Pressure: 50 Pa
- Temperature: 5000 K
- DSMC grid: 30 x 30 x 30
- DSMC particles: about 275k with `fnum 7.0e10`
- DSMC timesteps per ablation step: 20
- Ablation steps: 60 by default
- Recession per ablation step: about 0.8 voxel layers
- Recession model: kinetic-theory mass flux from diffuse O2 surface collisions
- Timestep control: kinetic-theory `mass-courant`, no prescribed `qkin`

## Run

From this directory:

```sh
mkdir -p output
../../build-dsmc/bin/dsmc-iac -log output/log.sparta -in in.sphere
```

To change the benchmark size, edit the first setting in `in.sphere`:
`variable sphere_resolution equal 100`.

The other high-level controls are grouped near the top of `in.sphere`:
`ablation_steps`, `dsmc_steps_per_ablation`, and `voxel_courant`.

Each run writes one output folder with the geometry, particle, fluid, and
history files:

- `surface_*.vtp`: initial surface plus one surface after each ablation frame
- `voxels_*.vtu`: initial voxels plus one voxel dump after each ablation frame
- `particles_*.dump`: initial particles plus one particle dump after each DSMC burst
- `fluid_*.vtu`: one DSMC grid snapshot after each DSMC burst
- `history.csv`: IAC recession history

## Timing on this machine

Serial timings from `/usr/bin/time -p` using this input with only
`sphere_resolution` changed:

| diameter voxels | initial active voxels | wall time | final active voxels | final voxel volume |
| ---: | ---: | ---: | ---: | ---: |
| 50 | 65,752 | 54.26 s | 686 | 1.04% |
| 75 | 221,119 | 113.77 s | 11,135 | 5.04% |
| 100 | 523,984 | 234.33 s | 50,148 | 9.57% |
