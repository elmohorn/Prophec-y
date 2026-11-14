# Prophec-Y reusable hardware UI kit (JUCE v2)

**Status:** native component implementation and optional standalone gallery on
`design/reusable-hardware-kit-v2`. **Not installed into PluginEditor**, not
merged to `main`, and not yet visually approved from a compiled JUCE capture.
Approved concept art is the design reference, **not a pixel-accurate texture
atlas**. All geometry and drawing in these files are procedural and themeable.

## What is implemented

| Component family | JUCE class | Variants and behaviour |
| --- | --- | --- |
| Hero, L, M, S, micro, nano dials | `HardwareDial` | 64/48/36/24/18/14 reference sizes, continuous value, 25-segment red ring, optional bipolar neutral position |
| Alternate dial caps and encoder/macro | `HardwareDial`, `HardwareEncoder` | brushed, soft black, knurled, scalloped; same parameter contract |
| Long, compact and mini vertical faders | `HardwareFader` | 80/56/40 reference track heights |
| Horizontal, compact and trim sliders | `HardwareFader` | 96/72/48 reference track widths |
| Rectangular, square, round, utility, under-glass, step buttons | `HardwareButton`, `HardwareStepButton` | selectable toggle, hover, focus, momentary mode using JUCE button APIs; playback indicator on steps |
| On/off switch | `HardwareSwitch` | JUCE ToggleButton, APVTS ButtonAttachment compatible |
| Mechanical mini toggle / stepped rotary selector | `HardwareMiniToggle`, `HardwareRotarySelector` | independent JUCE controls for binary and stepped parameters |
| Three-position switch | `HardwareThreePosition` | ComboBox A/B/C; use APVTS ComboBoxAttachment for discrete params |
| Generic dropdown | `HardwareDropdown` | JUCE ComboBox with dark popup |
| Status lamps | `HardwareLamp` | red/amber/green/blue/silver, setState |
| Screw / frame | `HardwareScrew`, `paintPlate` | decorative only |
| LED/VU meters | `HardwareMeter` | stereo segmented, analogue, compact bar; setLevels |
| Signal and modulation scopes | `HardwareSignalDisplay` | spectrum, oscilloscope, morph or generic envelope; setSamples |
| ADSR envelope graphic | `HardwareEnvelopeDisplay` | setAdsr on normalized display values |
| X/Y performance surface | `HardwareXYPad` | normalized (x,y), mouse + arrow keys, `onChanged` callback |
| OLED/preset strip | `HardwarePresetStrip` | bank/name, click or arrow key navigation callbacks |
| LFO wave options | `HardwareWaveSelector` | six exclusive shapes, selection callback |
| Module navigation tabs | `HardwareTabs` | dynamic exclusive tabs, selection callback |
| Matrix row | `HardwareModulationRow` | source, destination, amount, add/remove callbacks |
| Progress/load indicator | `HardwareProgress` | setProgress in normalized range [0,1] |

All controls share `HardwareTheme.h`: dark graphite/black, silver metal,
segmented red activation and a single cyan display accent; no green as a
decorative knob colour. **Reference sizes are design pixels**, not literal
hit-target limits: enclosing component bounds should be padded to at least
28×28 for mouse/focus access and enlarged for touch if required. Text labels
belong to the **parent layout**, not to a bitmap or control texture.

## Build the separate gallery

With JUCE 9.0.4 available via `P6_JUCE_PATH` (or the project pin):

```sh
cmake -S . -B build/gallery -DP6_BUILD_PLUGIN=ON -DP6_BUILD_HARDWARE_GALLERY=ON -DP6_JUCE_PATH=/path/to/JUCE
cmake --build build/gallery --target P6HardwareGallery --config Release
```

The `P6HardwareGallery` GUI app displays the controls with illustrative
values and user interactions. **It has no audio, DSP telemetry, host
automation or preset persistence**. It intentionally does not modify the VST
and can be run independently of a DAW. With no JUCE checkout/compiler in the
authoring environment, no native build, visual screenshot, pluginval, or DAW
test has been claimed.

## Use in the real editor

```cpp
#include "ui/hardware/HardwareDisplays.h"

p6plugin::hardware::HardwareDial cutoff{
    p6plugin::hardware::DialSize::hero,
    p6plugin::hardware::Cap::softBlack
};
p6plugin::hardware::HardwareFader mix{
    p6plugin::hardware::SliderSize::compactVertical
};
p6plugin::hardware::HardwareDropdown waveform;
p6plugin::hardware::HardwareMeter stereoMeter{
    p6plugin::hardware::MeterKind::stereoLed
};

// Unchanged existing APVTS parameter IDs, domains and host gestures:
auto cutoffBinding = std::make_unique<
    juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "EXISTING_PARAMETER_ID", cutoff);

// If desired, call cutoff.configure(...) exactly as for the project's
// ProphecyParameterControl to preserve exact-text editing and help.
// The UI kit never creates an additional parameter value or audio state.
// For meters, the editor calls setLevels(...) on the JUCE message thread.
```

Existing `ProphecyParameterControl` behaviour is inherited by
`HardwareDial` and `HardwareFader`: context menu, exact/raw editing, init
reset, keyboard gestures and existing APVTS SliderAttachment support.
`HardwareSwitch`/`HardwareButton` use ButtonAttachment;
`HardwareDropdown`/`HardwareThreePosition` use ComboBoxAttachment.
Attach only after allocating a control and before destroying it; control
objects must outlive their attachment objects.

For `HardwareXYPad`, a host editor should bracket connected parameter
changes with the appropriate begin/end gesture at drag boundaries and expose
the coordinates as **existing** parameters. Its `onChanged` callback does
not automatically provide a multi-parameter APVTS binding.

For UI-only visualizations use a snapshot or bounded queue to copy metrics
from the audio thread to the **message thread**. On the message thread,
downsample and call `HardwareSignalDisplay::setSamples` (max 2048 items).
Never call `repaint`, allocate vectors, or modify JUCE Components from the
audio callback. The gallery uses synthetic samples explicitly as **demo
data** and no FFT is implemented by the view class.

## Integration boundaries and next validation

1. Native **compile** with the pinned JUCE and CMake gallery flag; address
   compiler warnings/errors before reuse in PluginEditor.
2. Capture the gallery in light/dark host contexts at 100%, 125%, 150% OS
   scale; check labels/knobs/sliders for overlap and contrast.
3. Compare materials, segment illumination, background and size hierarchy
   against the two approved UI-kit reference images; **this procedural pass
   may differ from the photorealistic images**.
4. Check mouse/keyboard/focus, disabled states, popup obscuration, screen
   readers, zero crossing for bipolar LEDs and 28px minimum hit areas.
5. Integrate one control family at a time behind an editor flag; run state
   save/restore, automation, SysEx and pluginval in Windows DAWs before
   considering a merge. Keep the DSP/Character engine and `main` unchanged.

This folder is deliberately code, not a screenshot-only specification.
