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

expected = ["morph", "bulge", "pinch", "spikes", "tilt", "wobble", "attack", "release"]
params = {p["key"]: p for p in caps["chain_params"]}
assert all(k in params for k in expected)
assert all(params[k]["type"] == "float" for k in expected)
assert not any(k.startswith("mod_") for k in params), "modulation was removed"
canvas = params["shape"]
assert canvas["type"] == "canvas" and canvas["as_page"] is True
assert canvas["canvas_script"] == "canvas.js"
# preset_name because the page is its own preset browser and draws the name.
assert canvas["extra_keys"] == ["visual", "preset_name"]
assert params["visual"]["access"] == "read"

root = module["ui_hierarchy"]["levels"]["root"]
assert module["ui_hierarchy"]["pad_layout"] == "chromatic"
assert root["knobs"] == expected
assert root["params"][0] == {"key": "shape"}
assert [p["key"] for p in root["params"][1:9]] == expected
# The amp ADSR declares its viz group rather than relying on the detector, per
# docs/MODULES.md: detection is the fallback, declaring is the contract.
viz={p['key']:p.get('viz') for p in caps['chain_params']}
for k in ['attack','decay','sustain','release']:
    assert viz[k]=={'group':'amp','role':k}, (k,viz[k])
env=module['ui_hierarchy']['levels']['envelope']
assert env['knobs']==['attack','decay','sustain','release']
# The canvas page declares preset_browser and root carries the browser, so the
# two merge into one page -- and a preset browser is emitted before the level's
# grids, which is what puts the instrument's own picture first on load.
root_lvl=module['ui_hierarchy']['levels']['root']
assert root_lvl['list_param']=='preset' and root_lvl['count_param']=='preset_count'
assert params['shape']['preset_browser'] is True
assert 'presets' not in module['ui_hierarchy']['levels']
assert help_doc["title"] == "Bouba-Kiki"
assert help_doc.get("children") and all(p.get("lines") for p in help_doc["children"])
adapter = open("src/dsp/bouba_kiki_plugin.c").read()
assert "calloc(" not in adapter and "malloc(" not in adapter and "free(" not in adapter
print("PASS: module contract")
PY
