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
