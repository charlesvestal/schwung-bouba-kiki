#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build-host
cc -std=c11 -Wall -Wextra -Werror -Isrc/dsp tests/test_shape.c src/dsp/shape.c -lm -o build-host/test_shape
build-host/test_shape
