#!/bin/sh
set -eu

results_dir=${1:-results}
csv=${2:-sweep.csv}
rows_dir="${results_dir}/rows"
summary="${results_dir}/summary.csv"
tmp="${summary}.tmp"

mkdir -p "${results_dir}" "${rows_dir}"

printf '%s\n' 'case,tiff,temp,vx,ablation_time,mf_final,vf_final,mass_initial,mass_final,mass_loss,mass_loss_rate,vf_loss_rate,nvox_initial,nvox_final,status' > "${tmp}"

found=0
missing=0
tail -n +2 "${csv}" | while IFS=, read -r case_name tiff temp vx ablation_time _rest; do
  row="${rows_dir}/${case_name}.csv"
  if [ -s "${row}" ]; then
    tail -n +2 "${row}" >> "${tmp}"
    found=$((found + 1))
  else
    printf '%s,%s,%s,%s,%s,,,,,,,,,,missing\n' \
      "${case_name}" "${tiff}" "${temp}" "${vx}" "${ablation_time}" >> "${tmp}"
    missing=$((missing + 1))
  fi
done

mv "${tmp}" "${summary}"
completed=$(awk -F, 'NR > 1 && $15 == "ok" {n++} END {print n + 0}' "${summary}")
missing=$(awk -F, 'NR > 1 && $15 != "ok" {n++} END {print n + 0}' "${summary}")
printf 'Wrote %s from %d completed row(s), %d missing/failed row(s).\n' \
  "${summary}" "${completed}" "${missing}"
