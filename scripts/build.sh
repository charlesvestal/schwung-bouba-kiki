#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(dirname "$script_dir")"
image="schwung-bouba-kiki-builder"

if [[ "${NATIVE:-0}" != 1 && -z "${CROSS_PREFIX:-}" && ! -f /.dockerenv ]]; then
  if ! docker image inspect "$image" >/dev/null 2>&1; then
    docker build -t "$image" -f "$script_dir/Dockerfile" "$repo_root"
  fi
  docker run --rm -v "$repo_root:/build" -u "$(id -u):$(id -g)" -w /build "$image" ./scripts/build.sh
  exit 0
fi

cd "$repo_root"
rm -rf dist/bouba-kiki
mkdir -p build dist/bouba-kiki
if [[ "${NATIVE:-0}" == 1 ]]; then
  compiler="${CC:-cc}"
  arch_flags=()
else
  compiler="${CROSS_PREFIX:-aarch64-linux-gnu-}gcc"
  arch_flags=(-march=armv8-a -mtune=cortex-a72)
fi
"$compiler" -std=c11 -O3 -shared -fPIC "${arch_flags[@]}" \
  -fomit-frame-pointer -fno-stack-protector -DNDEBUG -Wall -Wextra -Werror \
  src/dsp/bouba_kiki_plugin.c src/dsp/shape.c src/dsp/voice.c src/dsp/synth.c \
  -Isrc/dsp -lm -o build/dsp.so
cp src/module.json src/help.json LICENSE NOTICE dist/bouba-kiki/
cp src/ui/canvas.js dist/bouba-kiki/canvas.js
cp build/dsp.so dist/bouba-kiki/dsp.so
chmod +x dist/bouba-kiki/dsp.so
tar -C dist -czf dist/bouba-kiki-module.tar.gz bouba-kiki
echo "built dist/bouba-kiki-module.tar.gz"
