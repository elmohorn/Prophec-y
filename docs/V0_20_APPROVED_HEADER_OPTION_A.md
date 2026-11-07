# v0.20 — Approved top console, OPTION A (7 Nov 2026)

## Approval source
Owner chose the **A** variant from the three-option header image, with one additional constraint: the long dark area must read as a **recessed, screwed-on hardware display**, not a floating black strip.

## Concrete visual contract
- A single aluminum mounting plate spans the top of the synth.
- **Prophec-Y logo sits on aluminum at far left, outside the black display.** No duplicated logo within the display.
- One dark inset screen to the right contains, in one horizontal line: **PRESETS | MASTER | OUTPUT L/R / PEAK/RMS | MIDI | RESET CLIP | MODULE/STUDIO**.
- Control groups are separated by fine rules within the *same* display; the meter is transparent to the parent screen and is not drawn as a separate green/black card.
- A metal bezel and visible screw heads around the display provide the requested recessed hardware look.
- The design must not obscure the first row of section labels (Oscillator 1 and Poly Mod).
- The header should be scaled using the same `designWidth=1460` horizontal transform as the JUCE controls; check widths 1240, 1460 and 1780.

## Deliberate limitations
This change does not alter DSP, FX chains, plugin state, SysEx, sample latency or preset storage. Manual Windows DAW screenshots and final owner visual approval are still required. An Actions screenshot only validates a specific revision; do not reuse images from previous PR commits.

## Regression
`tests/plugin_ui_smoke.cpp` checks layout containment and non-overlap for the nine controls in the display, separation of the physical logo, and the FX bay/Performance fixes. The generated screenshot artifact comes from the same commit as the code.
