# Bouba-Kiki Design

Bouba-Kiki is a four-voice polyphonic additive synthesizer for Schwung. One
shared contour morphs from rounded Bouba to pointed Kiki. The coefficients that
draw it also control 32 harmonic partials per voice, so picture and sound are
two expressions of one model.

## Interaction

Following MonkSynth, the root level contains a `type: "canvas"`, `as_page: true`
visualizer followed by the standard eight-knob page. Both bind, in order:
Morph, Bulge, Pinch, Spikes, Tilt, Wobble, Attack, Release. The visualizer draws
only a clean closed outline; Schwung retains header, footer, navigation, touch
feedback, and cached values. It performs no parameter reads while drawing.

Velocity sets voice level and visual pulse size. Note-ons pulse the single
outline; several notes reinforce it instead of creating more outlines. Pad
pressure temporarily pushes a voice toward Kiki and the shared display reflects
the strongest pressure. It never overwrites Morph.

## Synthesis

Morph crossfades curated endpoint spectra. Bulge reinforces broad low partial
groups; Pinch adds notches and odd/even contrast; Spikes raises high bands;
Tilt biases odd versus even partials; Wobble moves a gentle spectral emphasis.
The visual contour uses the same six normalized coefficients.

Targets update on parameter events and interpolate in audio rendering. The
callback allocates nothing and takes no locks. Voice stealing prefers the
quietest releasing voice, then the oldest active voice. Attack ranges from
immediate to soft, sustain is full while held, and Release ranges short to long.
Conservative normalization and soft limiting bound four-note Kiki chords.

## Verification

Tests cover four-note playback, stealing, envelopes, pressure, malformed and
clamped values, state round-trip, discontinuity and output bounds, synchronized
JSON/DSP defaults, the identical eight controls on both pages, bounded canvas
rendering, visibly rounded/pointed endpoints, native smoke loading, and ARM64
cross-compilation.
