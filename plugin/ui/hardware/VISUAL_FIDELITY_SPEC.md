# Prophec-Y UI kit — visual fidelity contract (v2.1)

**Status:** review contract, not approval of the native render. Scope: standalone JUCE gallery and reusable control library on `design/reusable-hardware-kit-v2`. No changes to `PluginEditor`, DSP, parameter IDs, or `main`.

## Ground-truth references

Two *previously approved* design boards, recovered from the project conversation's image library:

1. `Biblioteca UI Synth Industrial Prophec-Y v2.0.png` — first board, 18 numbered base components.
2. `Biblioteca de componentes PROPHEC‑Y v2.0.png` — extended board with display/composite components.

The illustrations are concept artwork, **not** 1:1 bitmap atlases, so the meaningful target is **indistinguishable hardware language, clear size hierarchy, correct mechanical geometry, colours and lighting**. Do not substitute the HTML/CSS screenshot as reference. The reference PNGs are not bundled in this repository; anyone conducting a pixel comparison needs the two separately approved source images.

## Fixed dimensions (logical design pixels at 100% UI scale)

| Family | Variants | Visible size/token | Minimum padded interaction space | Native class |
| --- | --- | --- | --- | --- |
| Dial | XL hero / L / M / S trim / micro / nano | diameters **64 / 48 / 36 / 24 / 18 / 14** | 28×28 or larger, plus ring clearance | `HardwareDial` |
| Alternate caps | brushed-metal / soft-black / knurled / scalloped | preserve selected base dial diameter | same as dial | `HardwareDial::setCap` |
| Encoder / stepped rotary selector | medium or large | chosen dial diameter; detents separately visible | 28×28 or larger | `HardwareEncoder`, `HardwareRotarySelector` |
| Vertical fader | long / compact / mini | travel **80 / 56 / 40** | travel length plus thumb clearance | `HardwareFader` |
| Horizontal slider | standard / compact / trim | travel **96 / 72 / 48** | travel length plus thumb clearance | `HardwareFader` |
| Square step button | numbered sequencer key | nominal 24×24 in board; larger hit zone | 28×28 or larger | `HardwareStepButton` |
| Compact OLED strip | preset display | 320×36 example from board | legible at host scale | `HardwarePresetStrip` |
| Under-glass preset button | compact long cap | 120×28 example from board | 28 px high at minimum | `HardwareButton(capacitive)` |
| Other mechanical parts | switch, lamp, utility, 3-way toggle, screw | proportions must match approved board | independent of visible face | their corresponding class |

For a nominal 14px Nano knob, **14px is the visible metal face**, not the touch target or the 25-segment ring diameter. A bigger component allocation must **never** change its nominal face diameter. Sliders have the same rule: their travel length is independent of the larger clickable container.

## Appearance rules (must remain consistent across every family)

- Chassis: near-black graphite with subtle brushed texture, recessed joints, thin machined borders, soft ambient shadow; **not** flat CSS cards.
- Caps: engineered metal rim + directional highlight + contact shadow; brushed metal genuinely silver, rubber black genuinely dark; knurled/scalloped show real **edge morphology**, not just one generic black circle.
- LED/active: controlled red/coral activation with red reflections **inside** the control; off state clearly physical and mostly unlit. Avoid a uniform exterior glow on every button. Micro/Nano may omit LED ring.
- Screen: cyan only for OLED/scopes; glass/recess remains black, and text must read as information. Green/amber on status/VU meters only when semantically meaningful.
- Labels: single title per module; no explanatory subtitles in the final VST layout. Gallery is allowed variant names for side-by-side review.
- Spacing: 8px base grid; at least 8px between visual component faces; 12–16px standard spacing; 16–24px module separation. Never let index/LED ring or label overlap neighbouring controls.
- State matrix: inactive, enabled, hover, pressed/toggled, keyboard focus, disabled; for dial additionally min, center, max, bipolar neutral where applicable.

## Per-component acceptance checklist

1. **Geometry:** face diameter or travel length equals token at 100% logical scale (allow ±1px rasterization rounding); sizes remain distinguishable.
2. **Safety area:** visible face, bezel, ticks, glow and focus ring do not clip at minimum allowable bounds. Hit target ≥28×28 even when decorative face is smaller.
3. **Material:** machined bevel, highlight direction and edge finish match the approved board at ordinary viewing size without excess bloom.
4. **Behaviour:** knob/fader change values, respond to host attachments, support focus and keyboard, and do not create independent DSP state.
5. **Scales:** gallery capture at 100%, 125%, 150% host UI scale; normalized dimensions preserved and text remains legible.
6. **Reference:** side-by-side crops from the **native JUCE** gallery and approved concept board, grouped by family; document each mismatch rather than interpreting the HTML mockup as proof.
7. **Integration:** only after passing native visual and interaction checks, land the family behind an editor feature flag and test automation/preset restore in a DAW.

## Implementation audit as of 2026-11-14

- **Confirmed from code before this revision:** native gallery used 100×94 bounding boxes for all six knobs, while its LookAndFeel painted a face equal to 76% of bounding-box width/height. Thus visible knob size depended on layout rather than nominal variant. Faders likewise shared oversize gallery boxes. The previous HTML screenshot had its own independent sizing errors.
- **Changed in v2.1:** `HardwareLookAndFeel` now paints using the specified nominal dial diameter and named fader-track length (with fit-down protection); `HardwareGalleryMain` sizes each demo by its actual geometry token. Cap materials now have more distinct face highlights and restrained brushed/rim treatment.
- **Still unverified:** no JUCE native build or gallery capture was available in the authoring environment; visual match, keyboard/attachment regression, VU needle, XY/composite accuracy, screenshots at scale, pluginval and DAW behaviour **must not be claimed as passed**.
- **Likely needs an additional graphics pass:** photoreal edge textures (especially knurled/scalloped), VU needle/housing, compact under-glass buttons, waveform/morph presentation, XY auxiliary hardware and preset screen typography. Prefer cached static surface textures plus vector overlays for live data/indicator motion rather than baking text or values into raster assets.

## Comparison log template

| Family / state | Approved reference crop | Native JUCE crop | Size tolerance | Material / light | Interaction | Decision |
| --- | --- | --- | --- | --- | --- | --- |
| XL hero @ 50% | required | pending | pending | pending | pending | not approved |
| L black + brushed @ 25%, 75% | required | pending | pending | pending | pending | not approved |
| M / S / micro / nano | required | pending | pending | pending | pending | not approved |
| Vertical 80/56/40 and horizontal 96/72/48 | required | pending | pending | pending | pending | not approved |
| Buttons, toggles, LEDs | required | pending | pending | pending | pending | not approved |
| OLED, scopes, VU, matrix, XY, ADSR | required | pending | pending | pending | pending | not approved |

**Do not integrate into the shipping VST until native gallery screenshot approval is explicit.**
