#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build-host
cc -std=c11 -O2 -Wall -Wextra -Werror -fPIC -shared -Isrc/dsp \
  src/dsp/bouba_kiki_plugin.c src/dsp/shape.c src/dsp/voice.c src/dsp/synth.c \
  -lm -o build-host/bouba-kiki-host.so
cc -std=c11 -O2 -Wall -Wextra -Werror -Isrc/dsp tests/smoke_host.c -ldl -lm -o build-host/smoke
build-host/smoke build-host/bouba-kiki-host.so
