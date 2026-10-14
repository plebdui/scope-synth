# Scope Synth

A polysynth you watch on a CRT. Each oscillator plays a curve (rose, heart,
spirograph, spinning cube...) the way oscilloscope music works: left = x,
right = y, one trip around the curve per cycle, so the key you play sets how
fast the beam goes round. The tube shows exactly what you hear.

- **2 oscillators**, each with a small preview of its shape and MORPH, OCT,
  SEMI, FINE, ROTATE, LEVEL and VOICES (1-8 copies detuned across 20 cents).
- **Shape lists:** an oscillator starts with one shape. `<` `>` pick the
  selected slot's shape, `+` adds a slot (right-click a slot to remove it),
  and MORPH blends through the slots in order.
- **Your own shapes:** NEW f(t) types a shape as `x = ...` and `y = ...`, with
  `t` going once around (0 to 2pi) and `u` = seconds for moving shapes, e.g.
  `x = sin(3t)`, `y = sin(4t + u)`. It joins the shape list and is saved with
  the project. Knows `+ - * / ^`, `sin cos tan asin acos atan sinh cosh tanh
  exp log sqrt abs floor ceil round sign pi e`, `pow min max atan2 mod`, and
  `3t` for `3*t`.
- **ENV** (attack, decay, sustain, release): every note's volume, and linkable.
- **LFO** (sine, triangle, saw, square, random): one for all notes, linkable.
- **Linking, like Serum:** drag the ENV or LFO handle onto a control. It gets a
  coloured range line and a tag; drag the tag up/down to set how much
  (negative works too), right-click it to unlink.
- **POLY / MONO** (16 notes), **LEGATO** (mono: overlapping notes don't restart
  the envelope and glide only between them), **GLIDE**, **VOLUME**.
- **Keyboard** along the bottom: click to play (lower on a key = harder),
  slide along the keys, `<` `>` change octave. Notes from your DAW light it up.
- **Everything automates:** every control, link amount, and each oscillator's
  shape list (A/B Shapes = how many slots, A/B Slot 1-8 = which shape) is a
  host parameter. Only the text of typed shapes isn't.
- Terminal look throughout (Liberation Mono, SIL Open Font License, in `fonts/`).

Formats: VST3 and CLAP (Windows, Mac), AU (Mac, GarageBand / Logic).

## Build

Windows, from Linux:

    make WINDOWS=true CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++

Mac: `make` (see `.github/workflows/macos.yml`). DPF goes in `dpf/` (pinned
commit and a small patch in the workflow).

Tests: `tests/engine_test.cpp` (the sound, on its own) and `tests/minihost.cpp`
(a tiny VST3 host run under Wine: plays notes, uses the window).
