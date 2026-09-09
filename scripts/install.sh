#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(dirname "$script_dir")"
device="${DEVICE:-ableton@move.local}"
dest="/data/UserData/schwung/modules/sound_generators/bouba-kiki"
[[ -d "$repo_root/dist/bouba-kiki" ]] || { echo "run scripts/build.sh first"; exit 1; }
ssh "$device" "mkdir -p '$dest'"
scp "$repo_root"/dist/bouba-kiki/* "$device:$dest/"
echo "installed to $dest; reload the chain for DSP changes and restart shadow_ui for canvas changes"
