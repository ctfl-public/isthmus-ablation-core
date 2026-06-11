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

You can skip this and pass paths directly to CMake instead:

```bash
cmake --preset dsmc -DDSMC_ROOT=/path/to/dsmc -DISTHMUS_ROOT=/path/to/isthmus
```

## Build DSMC/IAC

From a fresh clone:

```bash
cmake --preset dsmc
cmake --build --preset dsmc
ctest --preset dsmc
```

These preset commands require CMake 3.20 or newer. Check with:

```bash
cmake --version
```

If your machine has an older CMake, either load a newer module or use the
non-preset form:

```bash
cmake -S . -B build-dsmc -DIAC_DSMC_USE_OVERLAY=ON
cmake --build build-dsmc --target dsmc
ctest --test-dir build-dsmc --output-on-failure
```

The `dsmc` build preset:

- builds `libisthmus_ablation_core.a`;
- creates `build-dsmc/dsmc-overlay/src`;
- symlinks DSMC source files into that overlay;
- symlinks the IAC bridge command files into that overlay;
- generates the overlay `Makefile.package`;
- runs DSMC's normal `make <machine>` build there;
- writes `build-dsmc/bin/dsmc-iac`.

The default DSMC machine target is `mpi`. Override it with:

```bash
cmake --preset dsmc -DDSMC_MACHINE=mac_mpi
```

or without presets:

```bash
cmake -S . -B build-dsmc -DIAC_DSMC_USE_OVERLAY=ON -DDSMC_MACHINE=mac_mpi
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
ctest --preset dsmc
```

## Standalone Only

The standalone binary does not need DSMC:

```bash
cmake --preset standalone
cmake --build --preset standalone
ctest --preset standalone
```

Without presets:

```bash
cmake -S . -B build -DIAC_DSMC_USE_OVERLAY=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

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
voxels only and select `mass-fraction` as the first visible cell scalar.

## Rebuild The Manual

```bash
cmake --build --preset docs
```

The PDF is written to:

```text
docs/isthmus-ablation-core-manual.pdf
```
