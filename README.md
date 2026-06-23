<p align="center">
  <img src="docs/assets/iac-carbon-recession.gif" width="85%" alt="Voxel-resolved carbon geometry receding under DSMC-computed surface chemistry">
</p>

-----

# ISTHMUS Ablation Core

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)
[![DSMC/SPARTA coupled](https://img.shields.io/badge/DSMC%2FSPARTA-coupled-green.svg)](docs/development/build-and-link.md)

`isthmus-ablation-core` is a voxel-resolved solid ablation framework for
simulations where a material changes shape because gas-surface reactions, heat
fluxes, or prescribed recession rates remove solid mass.

The code treats the solid as a mass-bearing voxel ledger. Each voxel stores a
material, density, volume, remaining mass, and active/deleted state. Surface
fluxes can be applied directly to voxel faces or mapped through an ISTHMUS
triangle mesh, then carried back into the solid with conservative policies such
as normal-directed carryover. This makes it possible to model recession of
curved, rough, or image-derived samples without pretending the voxel stair-step
surface is the physical interface.

The main coupled workflow is simple: reconstruct a surface from a voxel solid,
let DSMC/SPARTA compute particle-surface chemistry and mass flux, remove the
corresponding solid mass, delete depleted voxels, rebuild the surface, and
repeat through physical time.

Manual PDF: [docs/isthmus-ablation-core-manual.pdf](docs/isthmus-ablation-core-manual.pdf)

## What It Does

- Evolves voxelized solids under surface-driven mass loss.
- Uses ISTHMUS to bridge voxel solids and triangle surface meshes.
- Couples to DSMC/SPARTA for rarefied gas chemistry, surface reactions, and MPI
  particle transport.
- Supports standalone verification cases with prescribed fluxes and exact
  expectations.
- Runs TIFF-derived carbon recession cases, including converged DSMC mass-flux
  loops.
- Writes history tables, VTK voxel/surface dumps, and regression-test outputs.

## Why It Exists

Ablating materials do not just lose mass; they change the geometry seen by the
gas. For porous or rough carbon-like solids, that moving boundary is the
problem. This repository supplies the solid-state memory and surface/voxel
coupling needed to let DSMC drive geometry change in a controlled, testable way.

## Coupled Workflow

1. Create or import a voxelized solid, such as a sphere, slab, or TIFF scan.
2. Use ISTHMUS to reconstruct a surface and map triangles to owning voxels.
3. Run DSMC/SPARTA around that surface, or apply a standalone prescribed flux.
4. Convert surface collisions, reactions, or imposed fluxes into removed solid
   mass.
5. Delete depleted voxels, update the voxel mass state, and rebuild the surface.
6. Repeat until the requested physical ablation time or recession state is
   reached.

## Modes

In standalone mode, `ia-core` runs lightweight SPARTA-style input files with no
gas domain. This is ideal for testing voxel recession, ISTHMUS mapping, TIFF
import, carryover policies, and exact solutions.

In DSMC-hosted mode, the same core library is compiled into a private
DSMC/SPARTA overlay. SPARTA owns particles, collisions, chemistry, MPI, and
surface tallies while this repository owns the evolving voxel solid.

## Quick Start

If DSMC and ISTHMUS are already installed, set their roots once:

```bash
export DSMC_ROOT=$HOME/dsmc
export ISTHMUS_ROOT=$HOME/isthmus
```

Then build the private DSMC/IAC executable:

```bash
make mpi
make test-dsmc
```

DSMC-linked builds require a DSMC checkout that includes the direct reaction
mass-flux computes:

```text
src/compute_react_surf_mass_flux.{h,cpp}
src/compute_react_boundary_mass_flux.{h,cpp}
```

Configure fails with a clear error if those files are missing. Standalone
`ia-core` builds do not require DSMC.

The top-level `make` targets wrap CMake configuration and DSMC's own machine
targets:

```bash
make mpi
make mac_mpi
make serial
make dsmc DSMC_MACHINE=<machine>
```

The coupled executable is:

```text
build-dsmc/bin/dsmc-iac
```

`dsmc-iac` is a compiled launcher, not a Python script. Python is used by
build/helper tools such as DSMC overlay generation and TIFF fixture generation,
but running coupled cases does not require a Python runtime in the launcher
path.

Run a coupled example:

```bash
build-dsmc/bin/dsmc-iac \
  -in examples/pregen-tiff-carbon-recession/in.dsmc-co-converge
```

This build does not edit your DSMC checkout. It creates a disposable overlay in
`build-dsmc/dsmc-overlay/` where DSMC source files and IAC bridge commands are
symlinked together.

For more setup detail, see the full [getting started guide](docs/getting-started.md)
and [build/link guide](docs/development/build-and-link.md).

## Standalone Mode

```bash
make standalone
make test-standalone
```

Run:

```bash
./build/ia-core -in examples/slab-direct-ablation/in.slab-direct-ablation
```

## Examples

- [Pregen TIFF carbon recession](docs/examples/pregen-tiff-carbon-recession.md)
- [DSMC sphere kinetic theory](docs/examples/dsmc-sphere-kinetic.md)
- [DSMC mass-flux coupling](docs/examples/dsmc-sphere-mass-flux.md)
- [Slab direct ablation](docs/examples/slab-direct.md)
- [Slab ISTHMUS ablation](docs/examples/slab-isthmus.md)
- [Sphere ISTHMUS ablation](docs/examples/sphere-isthmus.md)
- [TIFF sphere recession](docs/examples/tiff-sphere.md)

## Documentation

- [Documentation index](docs/index.md)
- [Manual PDF](docs/isthmus-ablation-core-manual.pdf)
- [Getting started](docs/getting-started.md)
- [Build and link guide](docs/development/build-and-link.md)
- [Architecture concept](docs/concepts/architecture.md)
- [Verification concept](docs/concepts/verification.md)
- [Code architecture](docs/development/code-architecture.md)
- [Directory layout](docs/development/directory-layout.md)
- [Testing](docs/development/testing.md)
- [Command reference](docs/commands/index.md)
- [Editor integration](editors/README.md)
- [MCC carbon array sweep](examples/mcc-carbon-array-sweep/README.md)

Useful command pages:

- [Language overview](docs/commands/language.md)
- [Voxel commands](docs/commands/voxel.md)
- [Voxel ablation](docs/commands/voxel-ablate.md)
- [Voxel dumps](docs/commands/voxel-dump.md)
- [Surface and flux commands](docs/commands/surface.md)
- [ISTHMUS surface reconstruction](docs/commands/isthmus-surface.md)
- [Grid/fluid VTU output](docs/commands/grid-write-vtu.md)
- [Looping and control flow](docs/commands/loops.md)
- [Verification commands](docs/commands/verify.md)

## Paths

You can pass roots directly:

```bash
make mpi DSMC_ROOT=/path/to/dsmc ISTHMUS_ROOT=/path/to/isthmus
```

The default DSMC machine target is `mpi`. Override it with:

```bash
make dsmc DSMC_MACHINE=mac_mpi
```
