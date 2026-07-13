# Sphere ISTHMUS Ablation

This example reconstructs a triangle surface from a voxel sphere with ISTHMUS,
applies a constant mass flux to all surface triangles, maps the triangle mass
loss back to voxels, and deletes depleted voxels.

The current case uses a 1 mm diameter sphere with `resolution 20`, meaning 20
voxels across the diameter. It uses `iac_timestep mass/courant 0.5 source q1`,
which gives `dt = 0.05625 s` for the current material, grid spacing, and flux.
The four-step test therefore runs to `0.225 s`.

## Inputs

```text
examples/sphere-isthmus-ablation/in.sphere-isthmus-local
examples/sphere-isthmus-ablation/in.sphere-isthmus-normal
```

The core loop is:

```text
isthmus_surface skin voxels solid resolution 1.6 iso 0.6
surf_flux skin source q1 select all
voxel_ablate solid surface skin policy carryover/normal delete yes
iac_run 1
```

This loop is intentionally command-driven. A future DSMC run can place DSMC
convergence, flux mapping, voxel ablation, and surface regeneration in the same
style of input-file loop.

## Run

```bash
build/ia-core -in examples/sphere-isthmus-ablation/in.sphere-isthmus-local
build/ia-core -in examples/sphere-isthmus-ablation/in.sphere-isthmus-normal
```

## Outputs

```text
output/sphere-isthmus-local/
output/sphere-isthmus-normal/
```

Open the VTU files in ParaView to inspect active voxel mass fraction. Open the
VTP files to inspect surface triangle area and the last applied surface mass
request.

## Verification

The automated wrappers are:

```text
tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-local-deletion.verify
tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-normal-carryover.verify
```

It compares the inferred continuum-equivalent radius (`rad`) and mass fraction against
the analytical shrinking-sphere solution:

```text
rad = rad0 - q1*time/rho
mf = (rad / rad0)^3
```

The local wrapper is the intentionally nonconservative reference path. It uses
percent tolerances of `7.0 percent` for `rad` and `20.0 percent` for mass
fraction at the final time.

The normal-carryover wrapper conserves mapped surface mass by pushing overshoot
inward through live neighbor voxels. It uses tighter percent tolerances of
`4.0 percent` for `rad` and `12.0 percent` for mass fraction, and it checks
that final-step dropped mass is essentially zero.
