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
cmake --preset dsmc
cmake --build --preset dsmc
ctest --preset dsmc
```

The preset commands require CMake 3.20 or newer. On older systems, either load
a newer CMake module or use:

```bash
cmake -S . -B build-dsmc -DIAC_DSMC_USE_OVERLAY=ON
cmake --build build-dsmc --target dsmc
ctest --test-dir build-dsmc --output-on-failure
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
cmake --preset standalone
cmake --build --preset standalone
ctest --preset standalone
```

Run:

```bash
./build/ia-core -in examples/slab-direct-ablation/in.slab-direct-ablation
```

## Paths

You can also pass roots directly:

```bash
cmake --preset dsmc \
  -DDSMC_ROOT=/path/to/dsmc \
  -DISTHMUS_ROOT=/path/to/isthmus
```

The default DSMC machine target is `mpi`. Override it with:

```bash
cmake --preset dsmc -DDSMC_MACHINE=mac_mpi
```

See [docs/getting-started.md](docs/getting-started.md) and
[docs/development/build-and-link.md](docs/development/build-and-link.md) for
details.
