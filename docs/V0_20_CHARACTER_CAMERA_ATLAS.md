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

`plugin/CMakeLists.txt` embeds this resource when present. The existing
`CharacterHardwareWindow::drawPhotoFrame` uses a narrower crop (448×97)
so the native JUCE meters remain independently rendered. The unchanged bezel
and glass are drawn after the photographic interior. The image generation
study's panel, knobs, lettering and FX display are deliberately excluded.

## Current limitations

- Optical Tremolo is a **visual preview**, not a working audio effect.
- Spring currently has energy-dependent reflections but not individual
  transparent moving coil layers; mounts remain fixed within the photo.
- This generator is deterministic for a particular source file. Final
  production QA requires actual native JUCE screenshots at 1240, 1460 and
  1780 widths, plus a bypass and pause/restart visual check.
- Do not merge the draft PR based solely on an image-crop preview.
