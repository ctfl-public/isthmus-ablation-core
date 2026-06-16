# isthmus-ablation-core

Voxel ablation and DSMC/ISTHMUS coupling core.

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
