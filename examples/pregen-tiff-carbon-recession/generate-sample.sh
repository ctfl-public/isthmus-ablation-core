#!/bin/sh
set -eu

cd "$(dirname "$0")"
mkdir -p output/pregen-tiff-carbon-recession
python3 ../../tools/make-carbon-sample-tiff.py \
  --width 24 \
  --height 24 \
  --depth 10 \
  --seed 20260611 \
  --out carbon-sample.tif
