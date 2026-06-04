# Build And Link

This page explains how `isthmus-ablation-core` is built in standalone mode and
how it is currently linked into DSMC for coupled ablation tests.

## What Is Linked

The project builds one core library:

```text
libisthmus_ablation_core.a
```

When ISTHMUS is found, that library is compiled with ISTHMUS support and links
against:

```text
libisthmus_cpp.a
```

The standalone executable is:

```text
ia-core
```

The DSMC executable links those same libraries and also compiles a small set of
bridge command files from this repository.

## Standalone With ISTHMUS

Point CMake at the user's ISTHMUS C++ build. The cleanest route is to set
`ISTHMUS_CPP_DIR` to the directory containing the `isthmus_cpp` CMake package:

```bash
ISTHMUS_CPP_DIR=/path/to/isthmus/build \
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The same setting can be passed as a CMake package directory:

```bash
cmake -S . -B build -Disthmus_cpp_DIR=/path/to/isthmus/build
cmake --build build
```

If ISTHMUS is found, CMake enables the ISTHMUS tests and examples. If ISTHMUS is
not found, the core can still build and run direct voxel tests that do not call
`isthmus surface`.

Run a standalone ISTHMUS example:

```bash
./build/ia-core -in examples/sphere-isthmus-ablation/in.sphere-isthmus-normal
```

This path uses only this repository's standalone parser. It does not require
DSMC.

## How DSMC Finds The Commands

DSMC already has a command-style registration mechanism. In
`src/input.cpp`, after built-in commands are checked, DSMC includes
`style_command.h` and expands every `CommandStyle(key,Class)` entry into parser
dispatch code.

The bridge headers in this repository contain those style macros:

```text
dsmc-bridge/voxel.h      -> CommandStyle(voxel,Voxel)
dsmc-bridge/isthmus.h    -> CommandStyle(isthmus,Isthmus)
dsmc-bridge/surface.h    -> CommandStyle(surface,Surface)
```

When these files are copied into `DSMC/src`, DSMC's generated
`style_command.h` includes them, and the existing parser recognizes:

```text
voxel ...
isthmus ...
surface ...
```

That is why we did not need to edit DSMC's parser logic. The current local
integration still changes the DSMC source tree in two practical ways:

- copy bridge source/header files into `DSMC/src`;
- update the selected DSMC makefile so it includes this repository's headers
  and links this repository's core library plus ISTHMUS.

So the precise statement is: no DSMC parser/source logic changes are required,
but DSMC must compile the bridge files and link the external libraries.

## Current DSMC Bridge Files

The bridge files are:

```text
dsmc-bridge/iacbridge.cpp
dsmc-bridge/iacbridge.h
dsmc-bridge/voxel.cpp
dsmc-bridge/voxel.h
dsmc-bridge/isthmus.cpp
dsmc-bridge/isthmus.h
dsmc-bridge/surface.cpp
dsmc-bridge/surface.h
```

They are intentionally thin:

- `voxel` parses DSMC input commands and owns voxel material/geometry/ablation
  calls into the core.
- `isthmus` calls the core to generate a surface from active voxels.
- `surface` installs the generated triangles into DSMC memory and reads DSMC
  `fix ave/surf` data back into triangle mass flux.
- `iacbridge` stores the active core model and performs the explicit surface
  installation/remapping work.

## DSMC Build

First build the standalone core with ISTHMUS:

```bash
ISTHMUS_CPP_DIR=/path/to/isthmus/build \
cmake -S /path/to/isthmus-ablation-core -B /path/to/isthmus-ablation-core/build
cmake --build /path/to/isthmus-ablation-core/build
```

Copy the bridge files into DSMC:

```bash
cp /path/to/isthmus-ablation-core/dsmc-bridge/* /path/to/dsmc/src/
```

Then update the DSMC machine makefile used for the build. For example, in
`/path/to/dsmc/src/MAKE/Makefile.mac_mpi`, add the core and ISTHMUS include
paths:

```make
SPARTA_INC = -D... \
  -I/path/to/isthmus-ablation-core/include \
  -I/path/to/isthmus/include
```

Add the static libraries:

```make
LIB = /path/to/isthmus-ablation-core/build/libisthmus_ablation_core.a \
      /path/to/isthmus/build/libisthmus_cpp.a
```

The local prototype currently also raises the DSMC compile standard:

```make
CCFLAGS = -g -std=c++20
```

This is a temporary convenience while the bridge and ISTHMUS/core interface are
still being shaped. A future user-facing package should either keep the bridge
within DSMC's supported standard or document the required compiler level
explicitly.

Build DSMC:

```bash
cd /path/to/dsmc/src
make mac_mpi
```

Run the coupled example from this repository:

```bash
/path/to/dsmc/src/spa_mac_mpi \
  -in /path/to/isthmus-ablation-core/examples/dsmc-sphere-kinetic/in.dsmc-sphere-kinetic
```

## DSMC Test Configuration

The default CMake build runs standalone/core tests only. To include DSMC-coupled
tests, point this repository at a built DSMC executable:

```bash
cmake -S . -B build-dsmc \
  -DIAC_DSMC_EXECUTABLE=/path/to/dsmc/src/spa_mac_mpi
cmake --build build-dsmc
ctest --test-dir build-dsmc --output-on-failure
```

That build runs the standalone tests plus DSMC-coupled tests. It does not build
DSMC itself; it only uses the executable specified by `IAC_DSMC_EXECUTABLE`.

Build the DSMC convergence report:

```bash
cmake --build build-dsmc --target dsmc-convergence-report
```

## End-User Simplicity

The current path is good enough for local development, but it is not yet as
simple as it should be for an outside user. A polished user workflow should
become one of these:

```bash
cmake -S . -B build \
  -DISTHMUS_ROOT=/path/to/isthmus \
  -DDSMC_SRC=/path/to/dsmc
cmake --build build --target install-dsmc-bridge
```

or a DSMC package/submodule layout where the user only enables a package and
points to ISTHMUS/core install prefixes.

Until that tooling exists, users need three path choices:

- `ISTHMUS_CPP_DIR` or `isthmus_cpp_DIR`: where CMake finds ISTHMUS for the
  standalone core.
- DSMC source path: where bridge files are copied.
- `IAC_DSMC_EXECUTABLE`: which DSMC executable this repository should use for
  coupled tests and reports.
