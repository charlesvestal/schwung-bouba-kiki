# Bouba-Kiki for Schwung

A four-voice additive shape synthesizer for Ableton Move via Schwung. A single
clean outline morphs from rounded **Bouba** to pointed **Kiki**, and the same
coefficients drive 32 harmonic partials in every voice.

## Controls

The visualizer and ordinary knob page share the same eight controls: Morph,
Bulge, Pinch, Spikes, Tilt, Wobble, Attack, and Release. Velocity controls
level and the outline pulse. Polyphonic pad pressure temporarily adds Kiki
brightness without changing the saved Morph value.

## Build and test

```bash
for t in tests/*.sh; do bash "$t"; done
./scripts/build.sh
```

The default build uses Docker to cross-compile `dist/bouba-kiki/dsp.so` for
Move's ARM64 CPU and creates `dist/bouba-kiki-module.tar.gz`. Use
`NATIVE=1 ./scripts/build.sh` for a host smoke build.

## Install

After building, run `./scripts/install.sh`. Set `DEVICE` to override the
default `ableton@move.local` target. Reload the chain after DSP changes;
restart `shadow_ui` after canvas changes because page drawers are cached.

MIT licensed. See `docs/plans/` for the design and implementation plan.
