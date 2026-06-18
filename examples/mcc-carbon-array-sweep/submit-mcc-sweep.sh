#!/bin/sh
set -eu

csv=${1:-sweep.csv}
max_running=${MAX_RUNNING:-2}
ntasks=${NTASKS:-128}
partition=${PARTITION:-normal}
account=${ACCOUNT:-coa_sjpo228_uksr}
time_limit=${TIME_LIMIT:-00:20:00}
dsmc_iac=${DSMC_IAC:-dsmc-iac}

if [ ! -f "${csv}" ]; then
  echo "Missing sweep CSV: ${csv}" >&2
  exit 1
fi

if ! command -v sbatch >/dev/null 2>&1; then
  echo "sbatch was not found. Run this on MCC login node or load SLURM first." >&2
  exit 1
fi

mkdir -p logs results/rows output

rows=$(awk 'END {print NR - 1}' "${csv}")
if [ "${rows}" -lt 1 ]; then
  echo "CSV has no data rows: ${csv}" >&2
  exit 1
fi

if awk -F, 'NR > 1 {print $2}' "${csv}" | while IFS= read -r tiff; do [ -f "${tiff}" ] || exit 1; done; then
  :
else
  echo "One or more TIFF cuts are missing. Run ./generate-cuts.sh first." >&2
  exit 1
fi

array_job=$(sbatch --parsable \
  --partition="${partition}" \
  --account="${account}" \
  --ntasks="${ntasks}" \
  --time="${time_limit}" \
  --array="1-${rows}%${max_running}" \
  --export=ALL,SWEEP_CSV="${csv}",DSMC_IAC="${dsmc_iac}" \
  run-row.sbatch)

gather_job=$(sbatch --parsable \
  --partition="${partition}" \
  --account="${account}" \
  --dependency="afterany:${array_job}" \
  gather-results.sbatch)

cat <<EOF
Submitted MCC carbon sweep.
  rows:           ${rows}
  concurrent:     ${max_running}
  ranks per row:  ${ntasks}
  array job:      ${array_job}
  gather job:     ${gather_job}

Watch:
  squeue -u \$USER

Results:
  results/rows/<case>.csv
  results/summary.csv
EOF
