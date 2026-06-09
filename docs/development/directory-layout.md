# Directory Layout

```text
include/isthmus_ablation/
  Public C++ headers for the core library.

src/
  Core implementation, parser, expression evaluator, and standalone CLI.

examples/
  Human-readable standalone input examples, organized as <case>/in.<case>.

dsmc-bridge/
  DSMC command-style bridge sources. The dsmc CMake target symlinks these into
  build-dsmc/dsmc-overlay/src; they should not be copied into the DSMC checkout.

tests/inputs/
  Regression wrappers, organized as <case>/in.<case>.verify.

docs/
  Manual, command reference, design notes, and generated diagram/PDF artifacts.

tools/
  Local documentation and development utilities.

build-dsmc/dsmc-overlay/
  Generated private DSMC source overlay. This is disposable build output.
```

Generated files are ignored:

```text
build/
build-dsmc/
output/
site/
```

The generated single-file manual is:

```text
docs/isthmus-ablation-core-manual.pdf
```
