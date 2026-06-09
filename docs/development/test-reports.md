# Test Reports

Verification reports are optional visual artifacts for inspecting regression
cases. They are not part of the core physics model.

## Command-Line Report Output

The executable can write verification data when the test runner asks for it:

```bash
build/ia-core -in tests/inputs/slab-direct-ablation/in.slab-direct-command.verify \
  -report-csv build/output/test-report-data/slab-direct-command-verification/report.csv
```

The CSV contains:

```text
quantity,expression,step,time,actual,exact,error,tolerance,tolerance-mode,norm,pass
```

This data-first design is intentional. Future DSMC/SPARTA-coupled runs can
emit the same verification/report CSV without depending on standalone plotting
code or modifying the input file.

## Report Cases

Reportable tests are listed in:

```text
tests/report-cases.csv
```

Each row gives the report index, test name, input file, and optional
requirements. Add future regression cases there when they should be available
to the report runner.

```csv
index,name,input,requires,expected-fail
2,slab-direct-yhi-verification,tests/inputs/slab-direct-ablation/in.slab-direct-yhi.verify,,no
3,slab-isthmus-finite-surface-verification,tests/inputs/slab-isthmus-ablation/in.slab-isthmus-finite-surface.verify,isthmus,no
4,slab-isthmus-ghost-wall-verification,tests/inputs/slab-isthmus-ablation/in.slab-isthmus-ghost-wall.verify,isthmus,no
5,sphere-isthmus-local-deletion-verification,tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-local-deletion.verify,isthmus,no
6,sphere-isthmus-normal-carryover-verification,tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-normal-carryover.verify,isthmus,no
7,sphere-isthmus-kinetic-theory-verification,tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-kinetic-theory.verify,isthmus,no
8,sphere-isthmus-normal-carryover-convergence,tests/inputs/sphere-isthmus-ablation/in.sphere-isthmus-normal-carryover-convergence.verify,isthmus,no
```

The runner auto-detects optional features from the CMake build cache. Cases
with unmet requirements are skipped when selecting `all`.
Cases marked `expected-fail` are allowed to return a verification failure, and
their CSV data is still included in the generated report.

## Build The Combined Report

Run all report cases and build one combined PDF:

```bash
cmake --build build --target test-report
```

The generated files are:

```text
build/output/test-report.tex
build/output/test-report.pdf
build/output/test-report-data/<test-name>/report.csv
```

The report includes:

- a table of contents for jumping to individual cases and input listings;
- a global summary table with case name, quantity, error, tolerance, norm, and pass/fail;
- one section per test case, starting on a new page;
- the test input listing for each case;
- any input files included by the test input, also starting on new pages;
- actual vs exact plots for each verified quantity;
- error plots with tolerance lines for each verified quantity;
- convergence order tables for convergence cases;
- convergence plots with each grid resolution shown in the legend.

## Direct Tool Use

The report runner accepts the same selection styles we expect to use as the
suite grows:

```bash
python3 tools/run-test-report.py all
python3 tools/run-test-report.py 1-4
python3 tools/run-test-report.py slab-direct-command-verification
python3 tools/run-test-report.py slab-direct-command-verification,slab-isthmus-ghost-wall-verification
```

Optional features can be overridden explicitly:

```bash
python3 tools/run-test-report.py all --available isthmus
python3 tools/run-test-report.py all --available ""
```

Names can be shortened if the partial name matches exactly one case:

```bash
python3 tools/run-test-report.py fix-verification \
  --out build/output/fix-report.tex
```

The lower-level report builder can still aggregate already-written CSV files:

```bash
python3 tools/build-test-report.py --discover build/output/test-report-data \
  --out build/output/discovered-report.tex --pdf
```

Discovery mode does not know input files unless named cases are provided, so
the normal workflow uses `tools/run-test-report.py`.

## Convergence Reports

When a test input uses the `convergence` command, the executable writes extra
CSV files next to the normal `report.csv`:

```text
convergence.csv
convergence-order.csv
convergence-<quantity>-<case>.csv
```

The report generator uses these files to add a convergence order table and
plots. The table records the first and last refinement values, the corresponding
errors, the measured order, the allowed order band, whether monotone error
reduction was required, and the pass/fail result. The plots show the
convergence error points and actual/exact time histories for each resolution
with legend entries identifying the varied input values.
