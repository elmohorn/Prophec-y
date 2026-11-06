# Character camera atlas — reproducing the photographic interiors

The approved hardware reference is the **existing Tube JUCE module**, not the alternative
black panel in the image-generation studies. Only the inside of the recessed camera may
change; the original Tube bezel, four screws, reflective glass, controls and layout remain.

## Source and output

Source: `Módulos de Tremolo y Spring Ópticos.png` (1817×866), approved photographic
Spring and Optical Tremolo camera studies. Keep the source outside the repository; the
source includes an unrelated black panel. Extract *only* the interior.

Generate (Python + Pillow):

```bash
python -m pip install Pillow
python tools/build_character_camera_atlas.py \
  --source "/path/to/Módulos de Tremolo y Spring Ópticos.png" \
  --output plugin/assets/character_cameras_atlas.jpg
```

The 448×360 JPEG contains three 448×120 rows in order:
1. Spring metal coils and physical mounts
2. Optical bulb darkened (physical socket and facing LDR retained)
3. Optical bulb illuminated

`plugin/CMakeLists.txt` embeds this resource when present. The camera
now uses the central **448×100 crop (source y=14..114)**, not the old 448×97
crop which cut off the lower Spring. A 582×130 scene inside the panoramic
cavity preserves the source's physical aspect ratio; any extra lateral space
belongs to the chassis, never stretched into the image. Native meters remain
independently rendered outside the illuminated scene. The original Tube
is restored to the **original `TubeChamber` component**, at its approved
320×180 bounds with the original single-row seven knobs, labels and
IN/OUT/CHANGE meters. Spring and Optical share its material language and
recessed/glass construction but use a wider camera to fit the physical
objects. The image generation study's alternate panel, knobs and lettering
are deliberately excluded.

## Reactive motion (DSP telemetry -> native camera)

The UI refreshes at **30 Hz**; all work is confined to the message thread. No
audio algorithm, parameter ID, preset codec or SysEx implementation changes.

- **Tube:** a smoothed physically inspired heater/bloom follows measured
  Character input RMS and *signal change caused by the effect* (not a random
  note-on blink). The original 12AX7 art and hardware frame are unchanged.
- **Optical Spring:** the DSP publishes three real delay-line tap RMS values
  (`DeviceTelemetry.values[2..4]`). The approved photograph contains **two**
  physical springs. Weighted combinations of taps (1+2, 2+3) set their
  independent energy and movement; do not synthesize a third nonexistent coil.
  **During atlas initialization only**, JUCE prepares a clean, stationary
  chassis plus two spatially feathered wire mattes with metal-specific chroma
  extraction. It removes the central wire silhouettes from the chassis using
  neighboring-row reconstruction. The **fixed end sockets and the front glass
  never move**. Each 30Hz frame requires exactly **two moving image draws**,
  down from 64 full-image transforms/clipped strips, so there are no hard
  per-segment seams. Oscillation amplitude follows actual measured tap power;
  the low-frequency motion is an illustration, not sample-accurate vibration.
- **Optical Tremolo:** remains clearly labelled **DESIGN PREVIEW** with no
  tremolo DSP or genuine physical LDR model. The JPEG is separated at load
  into a **static unlit chassis**, a narrowly confined bulb/beam illumination
  layer and an independently lagging LDR illumination layer. Only additive
  RGB differences between the matched dark/lit photographs are composited,
  bounded by the physical lamp/beam/sensor masks: **metal, screws and sockets
  do not flash**. **Three functional JUCE preview sliders** control LFO rate,
  modulation depth and bulb/LDR response, and
  an interactive waveform selector chooses sine, triangle, ramp or square.
  These are editor-local visual parameters only: **no APVTS attachment**,
  no host automation, DSP or preset mutation. The demonstration LFO is gated
  by actual final-output RMS, and the LDR illumination has delayed
  attack/release relative to the bulb. With silence, the hardware remains
  visible but the lamp fades. The old decorative fake knobs are removed.

All dynamic values are clamped and checked for non-finite telemetry, the
visual pause freezes the exact image, bypass clears energy immediately, and
the spring tail decays as real delay-line measurements fall. Cached JPEG
masks are prepared once, not recomputed in `paint()`.

## Medium-priority engineering hardening (2026-11-06)

### Time-based movement

The message-thread timer is still *requested* at 30 Hz but is no longer a
source of simulated time. `PluginEditor::timerCallback()` uses JUCE's monotonic
millisecond counter, clamps long host/UI stalls to 100 ms and feeds
`elapsedSeconds` into the preview LFO and image dynamics. Smoothing uses
`alpha = 1 - exp(-dt/tau)` with attack/release constants measured in **seconds**.
Motion therefore keeps the same pace at 30 and 120 UI refreshes per second.
Pausing MOTION continues to freeze the exact visual frame.

### Coherent real-time telemetry without audio-thread blocking

`plugin/RealtimeSnapshotMailbox.h` replaces independent atomic fields with
a **single-producer/single-reader triple-slot mailbox**. Each slot has atomic
ownership. The audio thread claims an unused slot, copies the full Character
snapshot including RMS, active effect type, pickups and scope values, then
publishes it with a release store. The UI claims that slot before copying
and releases it afterward. No torn cross-block frames or non-atomic races;
no mutex, heap allocation, busy waits or DSP feedback on the audio thread.
On a bounded failed read, the editor retains its last verified complete
snapshot and ages it out normally. Its revision comes from the **same slot**.

### Photo renderer responsibility

`CharacterPhotoLayers.h` owns atlas decode, one-time JPEG masks and static
chassis / two spring / bulb / LDR image compositing. The hardware window
owns the glass, machined aluminum bezel, meters and mode routing;
`CharacterVisualDynamics.h` owns measured-signal motion only.
All pixel manipulation runs on the message thread at initialization.

New `tests/character_telemetry_mailbox_probe.cpp` uses a concurrent writer
and reader to check 72 correlated values per frame without blocking.
The visual dynamics probe checks equivalent amplitudes/phase over a real
second at 30 Hz and 120 Hz, and caps delayed host ticks.
Five isolated local C++ mailbox stress runs passed without torn frames;
**the native JUCE build and DAW screenshot smoke remain unverified**
because GitHub Actions still fails before starting a runner.

## Priority-medium follow-up: timing, source quality and architecture (2026-11-06)

**UI clock after long DAW stalls.** The prior 100 ms hard cap discarded most
of a long elapsed interval and changed apparent Spring/Tremolo frequencies.
The message-thread timer now passes the entire non-negative monotonic elapsed
interval; `CharacterVisualDynamics` performs analytic attack/release
integration in seconds and uses double-precision modulo phase. No unbounded
frame-by-frame replay occurs, including after a one-minute pause. Existing
MOTION freeze/bypass semantics are retained. The pure C++ probe now checks
a full 2-second stall, a 60-second suspended interval, and stable 30/120 Hz
behavior. The optical design preview continues to be **visual-only**, not DSP.

**Image alpha provenance.** The lossy 448×360 JPEG can provide a workable
study/fallback, but chroma/brightness thresholding is not physically reliable
for silver wire against highlights. The pre-existing public APIs
`setSpringArtwork(cleanBackground, twoTransparentCoils)` and
`setOpticalArtwork(cleanChassis, darkFixture, transparentLampEmission,
transparentSensorEmission)` are now **preferred over an available JPEG
atlas**; previously the JPEG silently took precedence and could override
hand-authored RGBA images. Malformed artwork dimensions (must be 640x190) or missing alpha in moving/emissive layers are rejected, preserving the JPEG/procedural fallback. Explicit bulb and LDR alpha layers are animated
independently, leaving screws and metal stationary. The native smoke test
asserts identical results for explicit assets *with and without* a JPEG loaded.
Until approved RGBA layers are supplied, JPEG segmentation remains a
heuristic; do not claim that its limitations are fully eliminated.

**Renderer organization.** `CharacterProceduralArt.h` now contains the
vector-only fallback Tube, Spring and Optical art. The hardware window
retains only the physical bezel, native glass, compositing order, state and
meters. `CharacterHardwarePrimitives.h` shares the native screw and meter
painter with the procedural fallback rather than duplicating them;
`CharacterPhotoLayers.h` retains atlas decoding and matte creation;
`CharacterVisualDynamics.h` retains pure signal-to-motion integration.
The hardware header shrank from approximately 538 to 365 lines (including new artwork validation). All paths
are UI-thread-only; no audio processing, existing parameter IDs, presets,
state codecs or SysEx were modified.

**Validation:** the isolated C++ visual probe passes after the long-gap
change. JUCE/VST3 and full native screenshots remain unverified until
GitHub Actions can schedule jobs and the native smoke has been run.

## Verification

`P6CharacterVisualProbe` tests measured input/tap response, two coils
receiving three DSP pickups, continued vibration with sustained energy,
finite graphics state, decay in silence, exact freeze and immediate bypass.
`P6UiSmoke` exercises the original Tube camera, optical-preview state
isolation, **all seven** Spring APVTS attachments without modifying Tube
parameters, independent movement of both photographic coil bands, fixed
fastener pixels, undistorted 640×190/800×190 renders, separate bulb and LDR
light and an unchanged socket/chassis across brightness states. It exercises
the native JPEG masking and photo rendering path using a synthetic photo
fixture, with screenshots:

- `spring_photo_reactive_probe.png`
- `optical_audio_reactive_probe.png`

The real installed photographic atlas is included in the repository.
Native screens at 1240 / 1460 / 1780 widths, host/DAW rendering, motion
comparison and Windows/Linux builds still require operational GitHub
Actions runners. Do **not** substitute a generated visual mock for those
native screenshots or merge this draft PR based solely on a static image.
