#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build-host
cc -std=c11 -O2 -Wall -Wextra -Werror -Isrc/dsp tests/test_pitch.c src/dsp/shape.c src/dsp/voice.c src/dsp/synth.c -lm -o build-host/test_pitch
build-host/test_pitch
