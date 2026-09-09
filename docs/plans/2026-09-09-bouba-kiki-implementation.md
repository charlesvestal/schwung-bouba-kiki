# Bouba-Kiki Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a standalone four-voice Schwung additive synthesizer whose shared Bouba-to-Kiki shape drives both a fullscreen visualizer and its spectrum.

**Architecture:** A v2 C plugin owns four allocation-free additive voices and a shared parameter model. A module canvas page renders the same coefficient model, while the ordinary root knob page exposes the identical eight parameters.

**Tech Stack:** C11, Schwung plugin API v2, QuickJS-compatible JavaScript, JSON, POSIX shell tests, Docker ARM64 cross-compilation.

### Task 1: Contract skeleton

Create `src/module.json`, `src/help.json`, `README.md`, `LICENSE`, `.gitignore`,
and `tests/test_contract.sh`. First make the test require the ordered eight
parameters, canvas `as_page`, API v2 capabilities, and chromatic layout; see it
fail; add the minimum contract; see `PASS: module contract`; commit.

### Task 2: Shape coefficient model

Create `src/dsp/shape.{c,h}` and `tests/test_shape.{c,sh}`. Test endpoint
spectral centroids, clamps, normalized 32-partial targets, and deterministic
deformations before implementing endpoint tables and deformation mapping. Run
to green and commit.

### Task 3: Four-voice engine

Create `src/dsp/voice.{c,h}`, `src/dsp/synth.{c,h}`, and
`tests/test_synth.{c,sh}`. Test four notes, fifth-note stealing, release
preference, all-notes-off, envelopes, pressure brightness, finite samples, and
bounded output before implementing recurrence oscillators, smoothing,
envelopes, age tracking, stereo render, and limiting. Run to green and commit.

### Task 4: Schwung adapter

Create `src/dsp/plugin_api_v1.h`, `src/dsp/bouba_kiki_plugin.c`,
`tests/smoke_host.c`, and `tests/test_smoke.sh`. Test instance lifecycle, MIDI,
pressure, every set/get key, malformed values, state round-trip, and audio before
implementing the v2 adapter. Run to green and commit.

### Task 5: Clean-outline canvas

Create `src/ui/canvas.js`, `tools/preview_shapes.mjs`, and
`tests/test_canvas.sh`. Test hook registration, no draw-path reads, bounded
closed contours, and distinct endpoint pointiness. Implement frame-relative
polyline rendering, clock wobble, and MIDI/pressure pulse state. Generate and
inspect the endpoint/deformation contact sheet. Run to green and commit.

### Task 6: Build and package

Create `scripts/Dockerfile`, `scripts/build.sh`, `scripts/install.sh`,
`release.json`, `.github/workflows/release.yml`, and a packaging test. Model the
native/ARM64 paths on MonkSynth. Run `for t in tests/*.sh; do bash "$t"; done`,
run `./scripts/build.sh`, verify archive contents and binary architecture,
complete README documentation, and commit.

### Task 7: Review

Re-run from a clean build directory, inspect the rendered contact sheet, review
realtime safety/default drift/draw-path reads, then use
`superpowers:verification-before-completion` and
`superpowers:requesting-code-review` before reporting completion.
