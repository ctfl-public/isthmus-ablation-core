# Directory Layout

```text
include/isthmus_ablation/
  Public C++ headers for the core library.

src/
  Core implementation, parser, expression evaluator, and standalone CLI.

examples/
  Human-readable standalone input examples, organized as <case>/in.<case>.

dsmc-bridge/
  DSMC command-style bridge sources copied into /Users/tstoffel1/dsmc/src for
  local integration experiments.

tests/inputs/
  Regression wrappers, organized as <case>/in.<case>.verify.

docs/
  Manual, command reference, design notes, and generated diagram/PDF artifacts.

tools/
  Local documentation and development utilities.
```

Generated files are ignored:

```text
build/
output/
site/
```

The generated single-file manual is:

```text
docs/isthmus-ablation-core-manual.pdf
```
