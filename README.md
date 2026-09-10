# Bouba-Kiki for Schwung

A four-voice stereo contour-scanning synthesizer for Ableton Move via Schwung.
The published **Bouba** and **Kiki** contours are the neutral bases. Six
geometric controls deform them, including at both Morph endpoints. The DSP scans
the deformed X/Y contour directly; cross-modulation between contour scans creates
inharmonic colors. There is no detuned oscillator pair or separate sub oscillator.

Version 0.4 adds dual ADSR envelopes, a bipolar per-note modulation routing to
any shape control, and six factory presets. Version 0.3 replaced the 0.2
waveform-family engine with direct contour scanning.

## Controls

Sixteen controls across four pages.

**Shape** — the live outline, and the same six geometric controls as ordinary
knobs plus the modulation routing: Morph, Bulge, Pinch, Spikes, Tilt, Wobble,
Mod Amount, Mod Destination.

Morph interpolates the base contours. Bulge inflates lobes; Pinch squeezes deep
waists; Spikes grows narrow teeth; Tilt shears the contour; Wobble sends ripples
around it. The reference silhouettes are exact with the other controls neutral
(zero, except Tilt at 0.5), which is also the shipped default.

**Envelopes** — Amp ADSR on the top row, Mod ADSR on the bottom. Every note runs
both. The Mod envelope drives Mod Destination by the signed Mod Amount, moving
the shape per note without changing the saved knob position.

**Factory Presets** — six sounds: Pure Bouba, Kiki Knock, Slow Prickle, Rubber
Mouth, Glass Creature, Held Breath. Selecting one replaces all sixteen values.

Velocity controls level and the outline pulse. Polyphonic pad pressure
temporarily adds Kiki bite without changing the saved Morph value. The outline
follows the newest active note, drawing the modulation the DSP actually applied.

Each voice's bounded 256-point contour is updated per audio block and
interpolated during playback. Four-times oversampling and a six-pole lowpass
reduce scan aliasing; this is not a strictly bandlimited oscillator. Control
targets settle in roughly 24 ms. The drawing caches geometry, skips duplicate
pixels, and reads one compact telemetry value through Schwung's existing
staggered cache; direct knob turns are applied to that telemetry immediately, so
a stale poll never delays a gesture. Note events after the first page draw remain
latched until observed, then animate locally. Opening the page establishes a
baseline so old notes do not replay. The host's polling cadence still limits the
initial visual response to pressure and notes.

## Build and test

```bash
for t in tests/*.sh; do bash "$t"; done
./scripts/build.sh
```

The default build uses Docker to cross-compile `dist/bouba-kiki/dsp.so` for
Move's ARM64 CPU and creates `dist/bouba-kiki-module.tar.gz`. Use
`NATIVE=1 ./scripts/build.sh` for a host smoke build.

Regenerate the source-derived visual and DSP contour tables with
`node tools/compile_contours.mjs`, and the manifest and its matching DSP
contract with `node tools/compile_contract.mjs`; generated files are committed so
a release build does not require Node. `tests/test_generated.sh` checks them. `node tools/preview_shapes.mjs` renders previews.
For audio comparisons, install numpy and run
`python3 tools/render_sound.py build-host/bouba-kiki-host.so sounds-out --check`.
The optional spectral checks require numpy and cover the six geometric
controls, both envelopes, every modulation destination, and the presets. The
visual gallery in `shapes-out/sweeps.svg` shows five positions per knob at both
Morph endpoints. These checks supplement, rather than replace, listening.

## Install

After building, run `./scripts/install.sh`. Set `DEVICE` to override the
default `ableton@move.local` target. Reload the chain after DSP changes;
restart `shadow_ui` after canvas changes because page drawers are cached.

Software: MIT. Reference and derived contour artwork/data: CC BY-SA 3.0,
attributed in NOTICE. The 0.1 design in `docs/plans/` is historical; 0.2 replaced
the additive runtime with contour-derived wavetable playback; 0.3 replaced those
fixed families with direct contour scanning.
