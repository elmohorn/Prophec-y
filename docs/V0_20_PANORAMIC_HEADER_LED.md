# Panoramic glass header and segmented dials

Approved direction, 10 November 2026.

- One opaque, recessed smoked-glass fascia, without screws through the glass.
- Embedded Michroma wordmark, warm neon core with a restrained red edge.
- One capacitive preset selector containing both PRESETS and the current name.
  Clicking either part opens the opaque library below the glass. Long names
  scroll only when necessary. Parameter values appear briefly below the name.
- Raised Master, compact stereo output meter, and a quieter capacitive menu.
  MIDI input, Module/Studio and clip reset retain their existing actions.
- Twenty-five recessed red LED segments around rotary controls, at the
  reviewed 100% brightness. A slightly brighter final segment marks the value.
  LEDs and needle share the Slider's normalized position, including skewed
  ranges. Bipolar controls grow from semantic zero, with calibration applied.
- Audio processing, state schemas and parameter identifiers retain their
  existing contracts. No ring rendering runs on the audio thread.

The former waveform/context visualizer, spectrum and audio-frame capture
are preserved at commit `fe97f891ae10d88c2320c6fe6d1d2d659e2c994b` on branch
`archive/waveform-context-display-2026-11-10`. They are removed from the active
plugin and its build. The output level meter and Character telemetry remain.

The code review is backed by a deterministic LED geometry test and native
editor smoke checks for bindings, state compatibility, bounds, preset browser,
parameter readout, and nonblank software-rendered screenshots on both platforms.
Host-specific DPI, window placement and hardware MIDI remain manual checks.

## Candidate for future Character effects

BOSS RT-2 Rotary Ensemble is a visual reference for a possible dual-rotor
Character device. Its virtual-rotor lights separately indicate treble and bass
rotor speeds. A native version could visualize each actual DSP rotor and its
acceleration/deceleration. This is an option for evaluation, not an implemented
rotary effect. Source: https://www.boss.info/global/products/rt-2/
