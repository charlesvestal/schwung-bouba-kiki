#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
NATIVE=1 ./scripts/build.sh >/dev/null
archive=dist/bouba-kiki-module.tar.gz
test -f "$archive"
listing="$(tar -tzf "$archive")"
for file in module.json help.json canvas.js dsp.so LICENSE NOTICE; do
  grep -qx "bouba-kiki/$file" <<<"$listing"
done
file dist/bouba-kiki/dsp.so | grep -q 'shared library'
echo "PASS: packaging"
