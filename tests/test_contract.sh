#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

python3 - <<'PY'
import json

with open("src/module.json") as f:
    module = json.load(f)
with open("src/help.json") as f:
    help_doc = json.load(f)

assert module["id"] == "bouba-kiki"
assert module["component_type"] == "sound_generator"
assert module["api_version"] == 2
caps = module["capabilities"]
for key in ("chainable", "audio_out", "midi_in", "aftertouch"):
    assert caps[key] is True

expected = ["morph", "bulge", "pinch", "spikes", "tilt", "wobble", "mod_amount", "mod_destination"]
params = {p["key"]: p for p in caps["chain_params"]}
assert all(k in params for k in expected)
assert all(params[k]["type"] == "float" for k in expected[:-1])
assert params['mod_destination']['type']=='enum'
assert params['mod_amount']['min']==-1
canvas = params["shape"]
assert canvas["type"] == "canvas" and canvas["as_page"] is True
assert canvas["canvas_script"] == "canvas.js"
assert canvas["extra_keys"] == ["visual"]
assert params["visual"]["access"] == "read"

root = module["ui_hierarchy"]["levels"]["root"]
assert module["ui_hierarchy"]["pad_layout"] == "chromatic"
assert root["knobs"] == expected
assert root["params"][0] == {"key": "shape"}
assert [p["key"] for p in root["params"][1:9]] == expected
env=module['ui_hierarchy']['levels']['envelopes']
assert env['knobs']==['attack','decay','sustain','release','mod_attack','mod_decay','mod_sustain','mod_release']
assert module['ui_hierarchy']['levels']['presets']['list_param']=='preset'
assert help_doc["title"] == "Bouba-Kiki"
assert help_doc.get("children") and all(p.get("lines") for p in help_doc["children"])
adapter = open("src/dsp/bouba_kiki_plugin.c").read()
assert "calloc(" not in adapter and "malloc(" not in adapter and "free(" not in adapter
print("PASS: module contract")
PY
