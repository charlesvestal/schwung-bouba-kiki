# Bouba-Kiki for Schwung

A six-voice stereo **contour synthesizer** for Ableton Move via Schwung. The
published **Bouba** and **Kiki** silhouettes are the neutral bases; six geometric
controls deform them. The outline on screen and the sound are the same object:
the curve you see is the curve the oscillators read.

Version 0.4 adds an amplitude ADSR, six factory presets and six voices. A
per-note modulation envelope with a routable destination was built and then
removed: the two controls worth modulating carry their character in the
operator ratio, and moving that under a held note is heard as a glissando
rather than as timbre. Version 0.3 replaced the 0.2 waveform-family engine
with direct contour scanning.

## How the engine works

The shape is a closed 2D curve sampled at 256 points, so every point is an
`(x, y)` pair rather than a single sample. Three things follow from that.

**The curve is the operator waveform.** Each voice runs two phase accumulators,
and the second one's Y channel phase-modulates the first one's read position
with a one-sample feedback term — two-operator feedback phase modulation whose
operators are arbitrary geometry instead of sines.

The two operators do not read the same curve. The carrier scans the shape you
see; the modulator scans its own contour, sharpened relative to it. A
modulator's own harmonics multiply out into sidebands, so a spikier modulator
is a brighter result, and this is what gives Morph and pressure real range —
171 to 972 Hz for Morph — while both stay at a 1:1 ratio and therefore stay in
tune. The sharpening retreats, squared, as Pinch and Spikes open, because those
two earn their character from an irrational ratio and flooding the spectrum with
harmonic sidebands would dilute it; at their extremes the modulator is the
carrier's own contour again, bit for bit.

**One shape drives three FM parameters at once.** The geometric controls set
the operator waveforms, the operator frequency ratio and the modulation index
together, so sharpening the shape changes what the operators play, moves them
apart in frequency and drives them harder in a single gesture:

```
index = (.025 + .3*morph + .28*spikes + .22*pinch
         + .2*bulge + .2*pressure) * keytrack
ratio = 2 + 0.414*spikes + 1.732*pinch
```

The ratio is free-running and irrational, and only Spikes and Pinch reach it.
Two operators at an irrational ratio produce partials that never line up into a
harmonic series, which is the clangorous quality the instrument is for: at full
Pinch the off-harmonic energy is 0.68.

It starts at 2 rather than 1, and that is the difference between metallic and
out of tune. Sidebands land at `f*(1 - n*ratio)`, so for a ratio between 1 and 2
the first of them falls *below* the fundamental and takes the perceived pitch
with it — Pinch at 0.5 put the lowest strong partial 246 cents flat, and Spikes
at 1.0 put it 1508 cents flat. From 2 upwards every sideband folds back above
the fundamental. Measured across the full travel of both controls, the lowest
strong partial now sits on the note to within a cent, and the note's own
fundamental is the strongest partial throughout.

What matters is which controls are allowed to do that. Morph is the primary
axis and pressure is a continuous gesture under a held note, so either one
moving the tuning is heard as the note drifting rather than as timbre. Both are
kept out of the ratio and drive brightness through the index instead: Morph
sweeps the centroid 171 → 562 Hz with periodicity still at 0.990. Constraining
the ratio was tried instead — snapping it to whole numbers, to halves, and
crossfading neighbours — and every variant cost the character across all four
controls to fix a problem that only ever affected two. `tests/test_pitch.sh`
holds both halves of this: Morph, Bulge, Tilt, Wobble and pressure must stay
periodic, and Pinch and Spikes must not.

The contour is a wavetable, so its narrow teeth are high harmonics of it and
fold down at the top of the keyboard. It is smoothed by a width that follows
the note and the index is key-tracked, both inert below 420 Hz.

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

Ten controls across three pages.

**Shape** — the live outline, and the six geometric controls as ordinary knobs
alongside Attack and Release: Morph, Bulge, Pinch, Spikes, Tilt, Wobble.

Morph interpolates the base contours. Bulge inflates lobes; Pinch squeezes deep
waists; Spikes grows narrow teeth; Tilt shears the contour; Wobble sends ripples
around it. The reference silhouettes are exact with the other controls neutral
(zero, except Tilt at 0.5), which is also the shipped default.

Pinch and Spikes are the two that reach the operator ratio, so they are the two
that make the sound inharmonic — and the two that stay put while a note sounds.
Morph, Bulge, Tilt and Wobble hold the pitch exactly.

**Envelope** — one ADSR for level, drawn as a graph. Attack and Release also sit
on the shape page for quick reach; Decay and Sustain live here.

**Factory Presets** — six sounds: Pure Bouba, Kiki Knock, Slow Prickle, Rubber
Mouth, Glass Creature, Held Breath. Selecting one replaces all ten values.

Velocity drives the modulation index as well as the level, so playing harder
brightens rather than only getting louder, and it sets the outline pulse.
Polyphonic pad pressure adds bite and swells the level without changing the
saved Morph value. The outline follows the newest active note, drawing the
deformation the DSP actually applied.

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
