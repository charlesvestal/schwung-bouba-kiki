# Bouba-Kiki for Schwung

A four-voice stereo **contour synthesizer** for Ableton Move via Schwung. The
published **Bouba** and **Kiki** silhouettes are the neutral bases; six geometric
controls deform them. The outline on screen and the sound are the same object:
the curve you see is the curve the oscillators read.

Version 0.4 adds dual ADSR envelopes, a bipolar per-note modulation routing to
any shape control, and six factory presets. Version 0.3 replaced the 0.2
waveform-family engine with direct contour scanning.

## How the engine works

The shape is a closed 2D curve sampled at 256 points, so every point is an
`(x, y)` pair rather than a single sample. Three things follow from that.

**The curve is the operator waveform.** Each voice runs two phase accumulators
over the same curve. The second one's Y channel phase-modulates the first one's
read position, with a one-sample feedback term — two-operator feedback phase
modulation whose operators are arbitrary geometry instead of sines.

**One shape drives three FM parameters at once.** The geometric controls set
the operator waveforms, the operator frequency ratio and the modulation index
together, so sharpening the shape changes what the operators play, moves them
apart in frequency and drives them harder in a single gesture:

```
index = (.025 + .28*spikes + .22*pinch + .1*morph
         + .2*bulge + .2*pressure) * keytrack
ratio = a whole number, crossfaded with its neighbour,
         + .05*(morph + pinch + spikes)
```

The ratio is deliberately not free-running. An unconstrained ratio leaves the
two operators sharing no period at all, and the ear hears that as a lost
fundamental rather than as colour: sweeping Pinch used to drop the pitch a full
octave. Holding the ratio near a whole number keeps the note, and the 5% offset
keeps the clang — measured off-harmonic energy at full Pinch is 0.51 against
0.75 for a free ratio, with periodicity at the fundamental going from 0.06 to
0.87. The two neighbouring whole numbers are crossfaded rather than switched, so
no control steps as it sweeps. Pressure is excluded from the ratio entirely and
brightens through the index alone, because a ratio sliding under a held note is
heard as the note drifting out of tune.

Two pitch-dependent guards keep the top of the keyboard clean. The contour is a
wavetable, so its narrow teeth are high harmonics of it and fold back down as
grit above about C6: the contour is smoothed by a width that follows the note,
and the index is key-tracked. Both are inert below 420 Hz, and together they
take C6 from 5.3% to 1.2% of energy below the fundamental, and C7 from 6.0% to
0.3%. `tests/test_pitch.sh` guards the tuning and `tools/render_sound.py
--check` the folding.

Each voice also carries its own ripple phase, so a held chord moves internally
rather than breathing in lockstep.

**X and Y are the stereo pair.** The two channels are matrixed to left and right
(`.85x + .35y` / `.35x + .85y`), so the outline is traced across the stereo field
the way an oscilloscope in X/Y mode draws it. This is why Tilt, which shears Y
into X, reads as a change in stereo width rather than brightness.

Everything else is conventional: 4x oversampling through a six-pole lowpass
before decimation, a DC blocker per channel, linear ADSRs with quadratic time
mapping, and a rational saturator before the output clamp. There is no detuned
oscillator pair and no separate sub oscillator.

## Prior art

Most of this engine is established technique, and it is worth being precise
about which part is not.

The closed 2D curve as a waveform is the **polygonal / geometric oscillator**
line of work: Chapman & Grierson's n-gon waves (ICMC 2014), Hohnerlein, Rest &
Smith's continuous-order polygonal synthesis (ICMC 2016, DAFx-17), and Peschke &
Berndt's Geometric Oscillator (Audio Mostly 2017), whose Cyclone prototype
already made the edited shape *be* the timbre rather than a picture of it.
Argentieri & Scagliola's Arbitrary Polygon Oscillator (arXiv:2608.24726, DAFx
2026) generalizes this to morphing between arbitrary shapes and delivers x and y
as native stereo channels — the curve-as-waveform and the X/Y stereo pair used
here, arrived at independently. Further off: scanned synthesis (Verplank,
Mathews & Shaw, ICMC 2000) scans an evolving mass-spring system as a 1-D mono
wavetable, and wave terrain synthesis (Mitsuhashi, JAES 1982) orbits a 2-D
height field.

**E-RM's Polygogo** (2019) is the closest thing shipped, and the fair
comparison. Its stereo output is taken directly from the polygon's X and Y, its
Order control sets the ratio of the overtones, and it carries an FM operator
with its own ratio and index. Every ingredient here is on that front panel.

Two-operator feedback phase modulation with non-sinusoidal or wavetable
operators is likewise ordinary — Yamaha's RCM in the SY77 (1989) onward, and any
modern wavetable synth that can point FM at its own table.

What appears not to have been done before is the **coupling**. Polygogo has a
shape control, an FM ratio and an FM index, but they are three independent
panel knobs. Here they are one function: the geometry simultaneously defines the
operator waveforms, the modulator's frequency ratio and the modulation index, so
a single shape gesture moves timbre, harmonicity and brightness together and
cannot decouple them. That is a mapping, not a new DSP primitive, and it is the
only claim worth making.

The bouba/kiki framing rests on real work — Köhler (1929), Ramachandran &
Hubbard (2001), Adeli, Rouat & Molotchnikoff (2014) on timbre/shape
correspondence, and Ćwiek et al. (2022), which identifies spectral balance and
rise time as the governing acoustic parameters. Zacharakis, Velenis &
Cambouropoulos (CMMR 2025) already used a single slider morphing a contour from
smooth through rough to spiky as a timbre-matching stimulus. No synthesizer
appears to have been built on that axis; this one is not the first to notice it.

Yonatan Rozin's "The Contour Synthesizer" (2021) independently uses captured
object contours as waveforms, and shares this instrument's name.

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

Each voice's bounded 256-point contour is rebuilt per audio block and
crossfaded per sample, so geometry changes do not zipper; control targets settle
in roughly 24 ms. The oversampling above reduces scan aliasing, but this is not
a strictly bandlimited oscillator. The drawing caches geometry, skips duplicate
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
a release build does not require Node. `tests/test_generated.sh` checks them.
`node tools/preview_shapes.mjs` renders previews.
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
