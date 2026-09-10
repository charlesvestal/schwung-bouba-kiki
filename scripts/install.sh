#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(dirname "$script_dir")"
device="${DEVICE:-ableton@move.local}"
dest="/data/UserData/schwung/modules/sound_generators/bouba-kiki"
archive="$repo_root/dist/bouba-kiki-module.tar.gz"
[[ -f "$archive" ]] || { echo "run scripts/build.sh first"; exit 1; }
file "$repo_root/dist/bouba-kiki/dsp.so" | grep -q 'ARM aarch64' || {
  echo 'Refusing to install a native test binary. Run ./scripts/build.sh first.'; exit 1;
}
# Installs are serial, so any stage left over from an earlier run is stale.
# Pruning here bounds the rollback copies at one instead of one per install.
ssh "$device" 'rm -rf /data/UserData/bouba-kiki-install.*'
stage="$(ssh "$device" 'mktemp -d /data/UserData/bouba-kiki-install.XXXXXX')"
[[ "$stage" =~ ^/data/UserData/bouba-kiki-install\.[a-zA-Z0-9]+$ ]] || exit 1
# Drop the stage if we fail before the swap, but only once the module directory
# is back in place, so an interrupted swap keeps its rollback copy.
cleanup_stage() { ssh "$device" "test -d '$dest' && rm -rf '$stage'" || true; }
trap cleanup_stage EXIT
scp "$archive" "$device:$stage/module.tar.gz"
# Move whole directories, preserving loaded DSP inodes. Roll back if the new
# directory cannot be activated; keep the old package outside discovery paths.
ssh "$device" "set -eu
mkdir -p '$dest'
tar -xzf '$stage/module.tar.gz' -C '$stage'
for name in LICENSE NOTICE module.json help.json canvas.js dsp.so; do
  test -s '$stage/bouba-kiki/'\"\$name\"
done
trap 'if [ ! -d \"$dest\" ] && [ -d \"$stage/previous\" ]; then mv \"$stage/previous\" \"$dest\"; fi' EXIT
mv '$dest' '$stage/previous'
mv '$stage/bouba-kiki' '$dest'
rm '$stage/module.tar.gz'"
trap - EXIT
echo "installed to $dest; previous package retained in $stage/previous"
echo 'Reload the chain for DSP changes; restart shadow_ui for cached canvas changes.'
