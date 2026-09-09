#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

python3 - <<'PY'
import json

with open("src/module.json") as f:
    module = json.load(f)

assert module["id"] == "bouba-kiki"
assert module["component_type"] == "sound_generator"
assert module["api_version"] == 2
caps = module["capabilities"]
for key in ("chainable", "audio_out", "midi_in", "aftertouch"):
    assert caps[key] is True

expected = ["morph", "bulge", "pinch", "spikes", "tilt", "wobble", "attack", "release"]
params = {p["key"]: p for p in caps["chain_params"]}
assert all(k in params for k in expected)
assert all(params[k]["type"] == "float" for k in expected)
canvas = params["shape"]
assert canvas["type"] == "canvas" and canvas["as_page"] is True
assert canvas["canvas_script"] == "canvas.js"

root = module["ui_hierarchy"]["levels"]["root"]
assert module["ui_hierarchy"]["pad_layout"] == "chromatic"
assert root["knobs"] == expected
assert root["params"][0] == {"key": "shape"}
assert [p["key"] for p in root["params"][1:]] == expected
print("PASS: module contract")
PY
