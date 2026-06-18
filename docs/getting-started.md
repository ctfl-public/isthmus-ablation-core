# Getting Started

The easiest DSMC/IAC workflow is:

1. Install or build DSMC and ISTHMUS somewhere on your machine.
2. Tell this repository where they are.
3. Build a private DSMC executable inside this repository's build directory.

Your DSMC checkout is not modified. The build creates an overlay under
`build-dsmc/dsmc-overlay/` and writes the executable to:

```text
build-dsmc/bin/dsmc-iac
```

## One-Time Shell Setup

If DSMC and ISTHMUS live in stable locations, put these in `~/.bashrc`,
`~/.zshrc`, or your shell startup file:

```bash
export DSMC_ROOT=$HOME/dsmc
export ISTHMUS_ROOT=$HOME/isthmus
```

Then open a new terminal or source the file:

```bash
source ~/.zshrc
```

You can skip this and pass paths directly to `make` instead:

```bash
make mpi DSMC_ROOT=/path/to/dsmc ISTHMUS_ROOT=/path/to/isthmus
```

## Build DSMC/IAC

From a fresh clone:

```bash
make mpi
make test-dsmc
```

DSMC-linked builds require a DSMC checkout with direct reaction mass-flux
support:

```text
src/compute_react_surf_mass_flux.{h,cpp}
src/compute_react_boundary_mass_flux.{h,cpp}
```

If those files are missing, configuration stops before the overlay build. Use
standalone mode when you only need `ia-core` without DSMC coupling.

The top-level `make` targets provide a SPARTA-style front end while CMake still
handles dependency discovery and test registration underneath. The common DSMC
machine targets are:

```bash
make mpi
make mac_mpi
make serial
make dsmc DSMC_MACHINE=<machine>
```

Use `make check-mpi` or `make check-mac_mpi` to build and run the DSMC-hosted
test suite in one command.

Direct CMake builds are still possible for developers, but the documented
workflow uses the top-level `make` targets because they work on older CMake
installations such as MCC's default module.

### University Of Kentucky MCC

On the Morgan Compute Cluster, the default CMake module is sufficient for the
top-level `make` workflow:

```bash
module load cmake
```

Then point IAC at DSMC and ISTHMUS and build with DSMC's normal `mpi` machine
target:

```bash
export DSMC_ROOT=$HOME/TylerStoffel/dsmc
export ISTHMUS_ROOT=$HOME/TylerStoffel/isthmus

make mpi
make test-dsmc
```

MCC's system Python 3.6 is enough for the normal build and verification tests.
The optional PDF documentation and report targets may need additional local
tools.

The DSMC/IAC build:

- builds `libisthmus_ablation_core.a`;
- creates `build-dsmc/dsmc-overlay/src`;
- symlinks DSMC source files into that overlay;
- symlinks the IAC bridge command files into that overlay;
- generates the overlay `Makefile.package`;
- runs DSMC's normal `make <machine>` build there;
- writes `build-dsmc/bin/dsmc-iac`.

The default DSMC machine target is `mpi`. Override it with:

```bash
make mac_mpi
```

or:

```bash
make dsmc DSMC_MACHINE=mac_mpi
```

## Run A DSMC/IAC Case

```bash
build-dsmc/bin/dsmc-iac \
  -in examples/dsmc-sphere-kinetic/in.dsmc-sphere-kinetic
```

`build-dsmc/bin/dsmc-iac` quiets native SPARTA screen output by default and
prints compact IAC coupled summaries instead. Full SPARTA output is still
written to the log file. Add `--iac-dsmc-verbose` to show native SPARTA screen
output while debugging.

Compact `[IAC]` and `[SPA]` lines are colored by default when stdout is a
terminal. IAC lines are green and SPARTA lines are blue. Control this with:

```bash
build-dsmc/bin/dsmc-iac --iac-color auto -in examples/.../in.case
build-dsmc/bin/dsmc-iac --iac-color on -in examples/.../in.case
build-dsmc/bin/dsmc-iac --iac-color off -in examples/.../in.case
```

Color is only applied to terminal output; log files remain plain text.

Run the fast DSMC verification suite:

```bash
make test-dsmc
```

## Standalone Only

The standalone binary does not need DSMC:

```bash
make standalone
make test-standalone
```

Direct CMake builds are mostly useful when debugging the build system itself.
For normal use, prefer the `make standalone` and `make test-standalone`
targets above.

Run the first standalone example:

```bash
./build/ia-core -in examples/slab-direct-ablation/in.slab-direct-ablation
```

## Visual Outputs

Examples write under `output/` by default. For the direct slab example:

```text
output/slab-direct-ablation/history.csv
output/slab-direct-ablation/voxels_000000.vtu
output/slab-direct-ablation/voxels_000001.vtu
...
```

Open VTU/VTP files directly in ParaView. The examples usually write active
voxels only and select `mf` as the first visible cell scalar.

## Rebuild The Manual

```bash
make docs
```

The PDF is written to:

```text
docs/isthmus-ablation-core-manual.pdf
```
