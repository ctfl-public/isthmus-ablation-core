# MCC Carbon Array Sweep

This example is a small SLURM job-array sweep for the Morgan Compute Cluster.
It runs ten independent DSMC/IAC carbon recession cases from `sweep.csv`, with
each row using a pre-generated rough carbon TIFF, gas temperature, inflow speed,
and physical ablation time.

The workflow is intentionally simple:

1. Build `dsmc-iac` from the repository root.
2. Change into this folder.
3. Generate the TIFF cuts.
4. Submit the sweep.
5. Wait for the array and gather jobs to finish.
6. Read `results/summary.csv`.

```bash
cd examples/mcc-carbon-array-sweep
./generate-cuts.sh
./submit-mcc-sweep.sh
```

The submit script uses a SLURM array throttled to two concurrent rows by
default:

```bash
sbatch --array=1-10%2 run-row.sbatch
```

That means SLURM keeps at most two cases running at once and launches the next
row as soon as one finishes. By default each row uses one task on the `short`
partition, which is the validated mode for this demonstration. Override the
defaults without editing the scripts:

```bash
MAX_RUNNING=4 NTASKS=1 PARTITION=short ACCOUNT=coa_sjpo228_uksr ./submit-mcc-sweep.sh
```

This example currently uses array-level parallelism: many independent rows run
at the same time, but each coupled DSMC/IAC row runs serially. The row script
can launch through MPI, but the fully coupled TIFF recession loop still needs a
separate bridge fix before 128-rank row runs should be treated as supported. To
experiment with MPI row execution after that fix, override both the task count
and launcher:

```bash
MPI_LAUNCHER="mpirun -np 128" ./submit-mcc-sweep.sh
```

Each row writes an independent file in `results/rows/`. The gather job merges
those row files into `results/summary.csv`, so parallel jobs never append to one
shared table.

To rebuild the TIFF fixtures:

```bash
./generate-cuts.sh
```

The TIFF files are generated locally in this folder and are not tracked by git.
The SLURM jobs themselves do not need Python; the generator is only for creating
or refreshing the small input stacks before submission.
