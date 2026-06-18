#!/bin/sh
#SBATCH --job-name=iac-gather
#SBATCH --partition=normal
#SBATCH --account=coa_sjpo228_uksr
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --time=00:10:00
#SBATCH --output=logs/gather-%j.out
#SBATCH --error=logs/gather-%j.err

set -eu

cd "${SLURM_SUBMIT_DIR}"
./gather-results.sh results sweep.csv
