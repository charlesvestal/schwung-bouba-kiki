#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build-host
cc -std=c11 -O2 -Wall -Wextra -Werror -Isrc/dsp tests/test_wrap.c src/dsp/shape.c -lm -o build-host/test_wrap
build-host/test_wrap
