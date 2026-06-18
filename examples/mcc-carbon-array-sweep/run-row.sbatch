#!/bin/sh
#SBATCH --job-name=iac-row
#SBATCH --partition=normal
#SBATCH --account=coa_sjpo228_uksr
#SBATCH --nodes=1
#SBATCH --ntasks=128
#SBATCH --exclusive
#SBATCH --time=00:20:00
#SBATCH --output=logs/row-%A-%a.out
#SBATCH --error=logs/row-%A-%a.err

set -eu

cd "${SLURM_SUBMIT_DIR}"

csv=${SWEEP_CSV:-sweep.csv}
row_index=${SLURM_ARRAY_TASK_ID:?SLURM_ARRAY_TASK_ID is required}
dsmc_iac=${DSMC_IAC:-dsmc-iac}

row=$(awk -F, -v n="${row_index}" 'NR == n + 1 {print; found=1} END {if (!found) exit 1}' "${csv}")

case_name=$(printf '%s\n' "${row}" | awk -F, '{print $1}')
tiff=$(printf '%s\n' "${row}" | awk -F, '{print $2}')
temp=$(printf '%s\n' "${row}" | awk -F, '{print $3}')
vx=$(printf '%s\n' "${row}" | awk -F, '{print $4}')
ablation_time=$(printf '%s\n' "${row}" | awk -F, '{print $5}')
fnum=$(printf '%s\n' "${row}" | awk -F, '{print $6}')
seed=$(printf '%s\n' "${row}" | awk -F, '{print $7}')

outdir="output/${case_name}"
rowdir="results/rows"
result="${rowdir}/${case_name}.csv"
tmp="${result}.tmp"

mkdir -p "${outdir}" "${rowdir}" logs

echo "[IAC sweep] row ${row_index}: ${case_name}"
echo "[IAC sweep] tiff=${tiff} temp=${temp} vx=${vx} ablation_time=${ablation_time}"

srun -n "${SLURM_NTASKS}" "${dsmc_iac}" --iac-color off -in in.dsmc-co-converge-sweep \
  -var tiff "${tiff}" \
  -var temp "${temp}" \
  -var vx "${vx}" \
  -var ablation_time "${ablation_time}" \
  -var fnum "${fnum}" \
  -var seed "${seed}" \
  -var outdir "${outdir}"

history="${outdir}/history.csv"
if [ ! -s "${history}" ]; then
  echo "history file was not written: ${history}" >&2
  exit 1
fi

awk -F, -v case_name="${case_name}" -v tiff="${tiff}" -v temp="${temp}" \
  -v vx="${vx}" -v ablation_time="${ablation_time}" '
  NR == 2 {
    initial_mass = $5
    initial_nvox = $3
  }
  NR > 1 {
    final_nvox = $3
    final_mass = $5
    final_mf = $6
    final_vf = $7
  }
  END {
    if (NR < 2) exit 2
    mass_loss = initial_mass - final_mass
    printf "case,tiff,temp,vx,ablation_time,mf_final,vf_final,mass_initial,mass_final,mass_loss,mass_loss_rate,vf_loss_rate,nvox_initial,nvox_final,status\n"
    printf "%s,%s,%s,%s,%s,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%s,%s,ok\n",
      case_name, tiff, temp, vx, ablation_time,
      final_mf, final_vf, initial_mass, final_mass, mass_loss,
      mass_loss / ablation_time, (1.0 - final_vf) / ablation_time,
      initial_nvox, final_nvox
  }
' "${history}" > "${tmp}"

mv "${tmp}" "${result}"
echo "[IAC sweep] wrote ${result}"
