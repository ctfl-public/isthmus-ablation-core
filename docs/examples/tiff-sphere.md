# TIFF Sphere Example

This example imports a synthetic multipage TIFF sphere through the ISTHMUS TIFF
utility, reconstructs an ISTHMUS surface, applies a constant surface flux, and
maps the mass loss back to voxels with normal-directed carryover.

The committed fixture is:

```text
examples/tiff-sphere/sphere-24.tif
```

It is an uncompressed 8-bit grayscale TIFF stack with `24 x 24 x 24` samples.
ISTHMUS's TIFF utility treats pixel value `1` as active solid and value `0` as
void. The imported active bounding lattice reports as `23 x 23 x 23` for this
fixture because the outermost TIFF samples remain void. The fixture is generated
reproducibly with:

```bash
python3 tools/make-tiff-sphere.py --size 24 --radius 11.5 \
  --out examples/tiff-sphere/sphere-24.tif
```

## Input

The import command is:

```text
voxel_create solid tiff file examples/tiff-sphere/sphere-24.tif dx 5.0e-5 material carbon
```

The ablation loop is the same command-driven loop used by generated sphere
cases:

```text
iac_limit time ${ablation_time}
isthmus_surface skin voxels solid buffer 1 weighting no map yes
surf_flux skin source q1 select all
voxel_ablate solid surface skin policy carryover/normal delete yes
iac_run 1
iac_continue time ${ablation_time} variable keep
if "${keep} > 0" then "jump SELF ablate-loop"
```

## Run

```bash
cmake --preset standalone
cmake --build --preset standalone
./build/ia-core -in examples/tiff-sphere/in.tiff-sphere-normal
```

The example writes:

```text
output/tiff-sphere-normal/history.csv
output/tiff-sphere-normal/voxels_*.vtu
output/tiff-sphere-normal/surface_*.vtp
```

Open the VTU and VTP files in ParaView to inspect the imported voxel sphere and
the reconstructed surface as the sphere recesses.

## Verification

The automated wrapper is:

```text
tests/inputs/tiff-sphere/in.tiff-sphere-normal-carryover.verify
```

It disables visual dumps and compares final mass fraction and voxelized volume
fraction to the continuum shrinking-sphere solution. Because the sphere is
imported as arbitrary TIFF geometry, the exact expression computes an
equivalent initial radius from the imported initial mass:

```text
R0 = (3*initial-mass/(4*pi*rho))^(1/3)
mass-fraction = ((R0 - q1*time/rho) / R0)^3
```

This case verifies two things at once: the TIFF import path uses
`isthmus::utilities::load_active_voxels_from_tiff`, and the imported voxel state
can drive the same ISTHMUS surface-flux-to-voxel ablation loop as generated
geometries.
