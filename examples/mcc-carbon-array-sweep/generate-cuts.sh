#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "${script_dir}/../.." && pwd)

mkdir -p "${script_dir}/cuts"

i=1
while [ "${i}" -le 10 ]; do
  seed=$((20260618 + i))
  out=$(printf '%s/cuts/cut-%03d.tif' "${script_dir}" "${i}")
  python3 "${repo_root}/tools/make-carbon-sample-tiff.py" \
    --width 32 --height 32 --depth 16 --seed "${seed}" --out "${out}"
  i=$((i + 1))
done
