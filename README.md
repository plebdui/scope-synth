# Scope Synth

A polysynth you watch on a CRT. Each oscillator plays a curve (rose, heart,
spirograph, spinning cube...) the way oscilloscope music works: left = x,
right = y, one trip around the curve per cycle, so the key you play sets how
fast the beam goes round. The tube shows exactly what you hear.

- **2 oscillators**, each with a little CRT TV showing its shape.
  SHAPE picks the curve and morphs smoothly between neighbours (like a
  wavetable position), plus OCT, SEMI, FINE, ROTATE, LEVEL.
- **ENV** (attack, decay, sustain, release): every note's volume, and linkable.
- **LFO** (sine, triangle, saw, square, random): one for all notes, linkable.
- **Linking, like Serum:** drag the ENV or LFO handle onto any knob with a ring.
  The knob gets a coloured ring (how far it can move) and a tag under it.
  Drag the tag up/down to set how much (negative works too), right-click it to unlink.
- **POLY / MONO** (16 voices), **LEGATO** (mono: overlapping notes don't restart
  the envelope and glide only between them), **GLIDE**, **VOLUME**.
- **Keyboard** along the bottom: click to play (lower on a key = harder),
  slide along the keys, `<` `>` change octave. Notes from your DAW light it up.

Formats: VST3 and CLAP (Windows, Mac), AU (Mac, GarageBand / Logic).

## Build

Windows, from Linux:

    make WINDOWS=true CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++

Mac: `make` (see `.github/workflows/macos.yml`). DPF goes in `dpf/` (pinned
commit and a small patch in the workflow).

Tests: `tests/engine_test.cpp` (the sound, on its own) and `tests/minihost.cpp`
(a tiny VST3 host run under Wine: plays notes, uses the window).
