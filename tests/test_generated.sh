#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
repo="$PWD"
scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
mkdir -p "$scratch/src/ui" "$scratch/src/dsp"
cp src/ui/reference.svg src/ui/canvas.js "$scratch/src/ui/"
cp src/module.json "$scratch/src/"
(cd "$scratch" && node "$repo/tools/compile_contours.mjs")
(cd "$scratch" && node "$repo/tools/compile_contract.mjs")
for output in src/ui/canvas.js src/dsp/contour_tables.h src/dsp/contract.h src/module.json; do
  cmp "$output" "$scratch/$output"
done
echo 'PASS: generated visual and audio contours match source'
