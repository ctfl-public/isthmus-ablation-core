# isthmus-ablation-core

`isthmus-ablation-core` is a voxel-resolved solid ablation framework for
simulations where the shape of a material changes because gas-surface reactions,
heat fluxes, or prescribed recession rates remove solid mass.

The code treats the solid as a mass-bearing voxel ledger: each voxel has a
material, density, volume, remaining mass, and active/deleted state. Surface
fluxes can be applied directly to voxel faces or mapped through an ISTHMUS
triangle mesh, then carried back into the solid with conservative policies such
as normal-directed carryover. This makes it possible to model recession of
curved or rough voxelized samples without pretending the voxel stair-step
surface is the physical interface.

The main coupled workflow is:

1. create or import a voxelized solid, such as a sphere, slab, or TIFF scan;
2. use ISTHMUS to reconstruct a surface and map triangles to owning voxels;
3. run DSMC/SPARTA around that surface, or apply a standalone prescribed flux;
4. convert surface collisions, reactions, or imposed fluxes into removed solid
   mass;
5. delete depleted voxels, update the voxel mass state, and rebuild the surface;
6. repeat until the requested physical ablation time or recession state is
   reached.

The repository is designed to be useful in two modes. In standalone mode,
`ia-core` runs lightweight SPARTA-style input files with no gas domain, which is
ideal for testing voxel recession, ISTHMUS mapping, TIFF import, and exact
solutions. In DSMC-hosted mode, the same core library is compiled into a private
DSMC/SPARTA overlay so SPARTA owns particles, collisions, chemistry, MPI, and
surface tallies while this repository owns the evolving voxel solid.

Physically, the project is aimed at ablation and oxidation problems where a
porous or rough carbon-like solid recedes under rarefied gas exposure. It is not
a standalone flow solver; instead, it supplies the solid-state memory and
surface/voxel coupling needed to let DSMC drive geometry change in a controlled,
testable way.

Manual PDF: [docs/isthmus-ablation-core-manual.pdf](docs/isthmus-ablation-core-manual.pdf)

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

The top-level `make` targets wrap CMake configuration and DSMC's own machine
targets. The common machine targets are:

```bash
make mpi
make mac_mpi
make serial
make dsmc DSMC_MACHINE=<machine>
```

The executable is:

```text
build-dsmc/bin/dsmc-iac
```

Run a coupled example:

```bash
build-dsmc/bin/dsmc-iac \
  -in examples/dsmc-sphere-kinetic/in.dsmc-sphere-kinetic
```

This build does not edit your DSMC checkout. It creates a disposable overlay in
`build-dsmc/dsmc-overlay/` where DSMC source files and IAC bridge commands are
symlinked together.

## Standalone Mode

```bash
make standalone
make test-standalone
```

Run:

```bash
./build/ia-core -in examples/slab-direct-ablation/in.slab-direct-ablation
```

## Paths

You can also pass roots directly:

```bash
make mpi DSMC_ROOT=/path/to/dsmc ISTHMUS_ROOT=/path/to/isthmus
```

The default DSMC machine target is `mpi`. Override it with:

```bash
make dsmc DSMC_MACHINE=mac_mpi
```

See [docs/getting-started.md](docs/getting-started.md) and
[docs/development/build-and-link.md](docs/development/build-and-link.md) for
details.
