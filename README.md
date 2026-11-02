# Prophec-y — estado y descargas

Actualizado: 2 de noviembre de 2026.

## Descargar y probar

| Versión | Estado | Windows x64 VST3 | Linux x64 VST3 |
|---|---|---|---|
| [v0.20 preview 278](https://github.com/elmohorn/Prophec-y/releases/tag/v0.20.0-preview.278) | Cuatro nuevos Character FX + Tube/Browser | [Descargar ZIP](https://github.com/elmohorn/Prophec-y/releases/download/v0.20.0-preview.278/P6-Core-v0.20-prototype-Windows-VST3.zip) | [Descargar ZIP](https://github.com/elmohorn/Prophec-y/releases/download/v0.20.0-preview.278/P6-Core-v0.20-prototype-Linux-VST3.zip) |
| [v0.19.0](https://github.com/elmohorn/Prophec-y/releases/tag/v0.19.0) | Checkpoint de ingeniería y calibración provisional; editor de diagnóstico | [Descargar ZIP](https://github.com/elmohorn/Prophec-y/releases/download/v0.19.0/P6-Core-v0.19-Windows-VST3.zip) | [Descargar ZIP](https://github.com/elmohorn/Prophec-y/releases/download/v0.19.0/P6-Core-v0.19-Linux-VST3.zip) |

**Para probar el avance actual, descarga v0.20 preview 278.** Añade los cuatro módulos Character: **Plasma, Optical Spring, Ferrofluid y Neon Wavefolder**, junto a Tube. Cada uno tiene DSP propio, siete controles independientes guardables, ventana nativa reactiva, Mix/Output, bypass e inserción Start/End. Son prototipos para audición; el acabado visual y las pruebas musicales manuales en DAW siguen pendientes.

Los binarios están publicados en Releases, junto con SHA256SUMS.txt. No necesitas compilar ni buscarlos entre los artefactos temporales de Actions. El repositorio es privado: debes iniciar sesión en GitHub con una cuenta que tenga acceso.

La base de edición añade el contrato común de edición en las 37 instancias de controles de síntesis (34 parámetros distintos): valor exacto/raw, reset a Init, ajuste fino y teclado. Corrige las lecturas de envolvente según el perfil activo e incorpora fixtures de compatibilidad emitidos por codificadores históricos. [Detalle del bloque](https://github.com/elmohorn/Prophec-y/blob/v0.20-ui/docs/V0_20_PARAMETER_FOUNDATION.md). Se conserva la lectura de los estados anteriores y las huellas del motor. El estado actual usa codec v4 para guardar todos los dispositivos Character y Master; lee v1/v2/v3, conservando niveles históricos y la automatización antigua de None/Tube.

La preview 278 incorpora los cuatro [módulos Character y su alcance](https://github.com/elmohorn/Prophec-y/blob/v0.20-ui/docs/V0_20_CUSTOM_CHARACTER.md): Plasma con ignición/conducción; Optical Spring con tres captadores del tanque; Ferrofluid con filtro resonante sensible a la envolvente; Neon con wavefolding asimétrico. Reutiliza kernels MIT de DaisySP y el oversampler DSPark. Son diseños propios, sin afirmar emulación física verificada de pedales comerciales. El SysEx Prophet permanece intacto.

La preview 278 conserva el **OLED contextual**: al seleccionar OSC 1/2 muestra su ciclo nominal de shape/PW; al seleccionar una envolvente muestra el ADSR del motor, con gate y escala de tiempo. Son previews del modelo, antes de modulación, no osciloscopio ni mediciones del hardware. Las vistas contextuales de filtro, LFO y FX siguen pendientes; los medidores de salida ya están implementados. [Alcance, captura y validación](https://github.com/elmohorn/Prophec-y/blob/v0.20-ui/docs/V0_20_CONTEXT_DISPLAY.md).

La preview 278 implementa el diseño Character aprobado: **panel de aluminio independiente y válvula hundida en una cavidad oscura**, con marco mecanizado, circuito interior y luz cálida por capas ligada a la señal. Conserva bypass, inserción Start/End, plegado y los siete controles Tube. [Captura del editor real y detalle](https://github.com/elmohorn/Prophec-y/blob/v0.20-ui/docs/V0_20_RECESSED_CHARACTER.md).

La preview 278 añade **Master Output y medidores L/R de pico, RMS y CLIP**, después de toda la cadena. Instancias nuevas: **-18 dB**. Sesiones anteriores: **0 dB**, conservando su nivel; puedes bajar el Master para darles margen. El volumen Prophet se identifica ahora como PROGRAM VOL. Master introducido en codec v3; sesión actual v4 fuera del SysEx, rampa de 20 ms y aviso PRE antes del Master. No se añade limitador ni normalización de voces. [Detalle, migración y captura real](https://github.com/elmohorn/Prophec-y/blob/v0.20-ui/docs/V0_20_MASTER_OUTPUT.md).

La preview 278 incorpora el **browser local de presets**: Factory Init / Imported / User, búsqueda y filtros, favoritos persistentes, Save/Save As, protección de cambios sin guardar e import/export de programa/banco `.syx`. Cargar un preset conserva los controles globales, incluido Master; los dumps importados conservan sus bytes y orden. Factory trae nuestro Init, sin bancos Sequential. Tras reabrir un proyecto DAW se recupera el sonido, pero la asociación de biblioteca es transitoria: usa Save As hasta volver a seleccionar una entrada. [Uso, límites y captura nativa](https://github.com/elmohorn/Prophec-y/blob/v0.20-ui/docs/V0_20_PRESET_BROWSER.md).

### Instalación en Windows

1. Cierra tu DAW y descomprime el ZIP de Windows.
2. Localiza la carpeta completa **P6 Core.vst3** dentro de las subcarpetas extraídas.
3. Copia esa carpeta completa a `C:\Program Files\Common Files\VST3\`.
4. Abre el DAW, vuelve a escanear plugins y carga **P6 Core** como instrumento VST3.

En Linux, copia el bundle completo a `~/.vst3/` y vuelve a escanear. No se incluye aplicación standalone ni instalador. Ambas versiones usan la misma identidad de plugin: conserva una copia de la anterior fuera de la carpeta de plugins antes de reemplazarla. Factory SysEx y audios de terceros no se redistribuyen.

### Qué contiene cada versión

- **v0.19:** corrección de primera tecla Unison tras una cola Release, consistencia de dominios de parámetros y herramientas/comparaciones acústicas ampliadas. Default permanece congelado; no se declara paridad acústica completa con Prophet-6. [Cierre y límites](https://github.com/elmohorn/Prophec-y/blob/v0.19-sound-matching/docs/V0_19_CLOSEOUT.md).
- **v0.20 preview 278:** panel nativo de aluminio, controles de síntesis con valores semánticos, selectores FX A/B, diagnóstico RAW14, browser local, Master Output y cinco dispositivos Character con posición y bypass. Latencia fija del wrapper: **71 muestras**, incluso con None. Studio personalizado, Compare/A-B/undo, acabado visual final y pruebas musicales manuales en DAW siguen pendientes. [Estado del prototipo](https://github.com/elmohorn/Prophec-y/blob/v0.20-ui/docs/V0_20_TUBE_PROTOTYPE.md).

### Código y validación

`main` conserva la base de código v0.18; esta portada muestra el estado global y las descargas posteriores. El desarrollo no está integrado automáticamente en `main`.

| Código | Commit del binario publicado | Validación |
|---|---|---|
| [v0.19-sound-matching](https://github.com/elmohorn/Prophec-y/tree/v0.19-sound-matching) | `3e148b6c9d1002178ca294fc4613943fa7533ee6` | [CI aprobada](https://github.com/elmohorn/Prophec-y/actions/runs/41268532229) |
| [v0.20-ui](https://github.com/elmohorn/Prophec-y/tree/v0.20-ui) | `77e549f31d31cd93c702f862cfb7d0426f60db73` | [CI aprobada](https://github.com/elmohorn/Prophec-y/actions/runs/41430731561) |

Windows/Linux VST3, tests automatizados, pluginval strictness 10, sanitizers Linux y guards de análisis aprobados en las ejecuciones indicadas. v0.20 también incluye smoke nativo de editor/audio. Las releases reutilizan exactamente esos ZIP validados y verifican procedencia, checksums e integridad antes de publicarlos. La matriz de pruebas manuales en distintos DAW sigue pendiente.

[Estado actual de v0.20](https://github.com/elmohorn/Prophec-y/blob/v0.20-ui/docs/V0_20_CURRENT_STATUS.md) · [Revisión de producto y próximos pasos](https://github.com/elmohorn/Prophec-y/blob/v0.20-ui/docs/V0_20_PRODUCT_REVIEW.md).

La documentación técnica histórica de la base v0.18 continúa a continuación.

---

## Base técnica — phase 1 / v0.18

Compatibility-first C++20 core for a Prophet-6-style software synthesizer. The 1024-byte hardware program state remains canonical and lossless; DSP/calibration layers are replaceable so measured sound matching can evolve without breaking SysEx/NRPN compatibility.

## v0.18 headline — digital effects closed

v0.18 adds and hardens one global realtime-safe serial digital-effects path:

`voice sum -> program/MIDI volume -> analog-distortion candidate -> FX A -> FX B -> output`

- Verified legacy Effect A selectors now render Off, BBD Delay, Digital Delay, Chorus, PH1 and PH2.
- Verified legacy Effect B selectors now render Off, BBD Delay, Digital Delay, Chorus, PH1, PH2, Hall, Room, Plate and Spring.
- PH3, Ring Mod, FL1 and FL2 have internal candidate DSP and regression coverage, but their later-OS raw selector encoding is deliberately not guessed or serialized.
- FX tempo sync uses the existing internal/host/MIDI common clock rather than introducing another timing authority.
- Global bypass is bit-transparent at the DSP boundary.
- Effect-type changes invalidate stale tails; CC120 uses bounded/logical realtime invalidation rather than sweeping long buffers in the audio callback.
- Minimum deterministic parameter smoothing protects delay/rate/depth/feedback automation from obvious discontinuities.
- DSP fingerprints and a performance benchmark are permanent CI diagnostics.
- Safe optimisations now include reverb coefficient caching, exact zero-history FIR skipping, base-pitch conversion caching and Pan Spread gain caching.
- The dedicated nonzero-Pan render is bit-identical before/after Pan caching: `d0332e325eb4376a`.
- Windows/Linux VST3, CTest, `pluginval --strictness-level 10`, and Linux ASan+UBSan remain release gates.

See `docs/V0_18_FX_AUDIT.md`, `docs/V0_18_CLOSEOUT.md` and `docs/PROPHET6_PARITY_MATRIX.md`.

## v0.17 — control/performance contract

v0.17 closed the non-FX control/performance boundary before digital effects:

- versioned lock-free last-writer-wins host/MIDI edit ordering;
- sustain/Damper CC64 with physical-key state separated from sustained voice lifetime;
- MIDI Volume CC7 independent of saved program volume;
- distinct CC120/121/123 semantics;
- realtime allocation regression tests and allocation-free arp ordering;
- evidence-backed synced-LFO first-key phase reset while the exact rhythmic-division law remains unguessed.

See `docs/V0_17_CONTROL_CONTRACT.md` and `docs/V0_17_CLOSEOUT.md`.

## v0.16 — wrapper/release stability gate

v0.16 established the cross-platform release baseline: Windows/Linux VST3, strict pluginval, Linux sanitizers, release-mode tests, chord-memory held-key fixes and serialized-layout/cardinality integrity.

See `docs/V0_16_STABILITY.md`.

## Core architecture

```text
SysEx / MIDI NRPN / host automation
              |
      P6PatchState
   canonical 1024 bytes
              |
 ParameterMapper / calibration
              |
        P6Engine / DSP
              |
            Audio
```

The core remains JUCE-independent. JUCE 9.0.4 is an optional thin VST3 host adapter; `P6PatchState`, `P6PerformanceScheduler` and `P6Engine` remain the musical/state sources of truth.

## Compatibility / state

- Full 1024-byte Prophet-6 program payload preserved losslessly.
- Correct Program Data `0x02` vs Edit Buffer Data `0x03` handling.
- Sequential 7-bit pack/unpack and concatenated-bank scanning.
- Known serialized parameters map to verified NRPN identities while unsupported bytes remain untouched.
- Stateful NRPN decoder with Data Increment/Decrement support.
- NRPN edits update the canonical raw payload.
- Program-name NRPNs `236..255` update payload bytes `107..126` and visible name state.
- Sequence note/velocity NRPNs `256..1023` update stored sequence bytes in place.
- Unknown sequence high-bit metadata is preserved rather than interpreted by guesswork.
- Host state uses a versioned JUCE-independent codec and preserves unknown hardware bytes.

## Performance scheduler

- Internal sample-domain clock.
- Host BPM/PPQ anchoring.
- MIDI Clock 24 PPQN with Start/Continue/Stop behavior.
- Ten documented clock divisions.
- Arp Up, Down, Up+Down, Random and Assign, 1–3 octave range, HOLD and Beat Sync.
- Stored six-track / 64-step sequencer playback with middle-C transposition reference.
- Source ownership for live/ARP/SEQ notes so generated note-offs do not kill coincident live notes.
- Opaque swing ratio, sequence flag/mode semantics and exact Random generator remain explicitly unguessed.

## DSP core

- Six free-running voices.
- Continuous triangle -> saw -> pulse trapezoid-family oscillator morph with anti-alias correction.
- Sub-sample Osc2 -> Osc1 hard sync.
- Osc1 sub oscillator and noise source.
- Independent per-oscillator/per-voice slow Slop.
- Resonant two-pole HPF candidate.
- Resonant four-pole SSM2040-lineage LPF candidate.
- Mixer -> HPF -> LPF -> VCA voice path.
- Independent exponential amp/filter envelopes.
- Audio-rate-capable global LFO and aftertouch routing.
- Audio-rate Poly Mod from Filter Env/Osc2 to Osc1 pitch/shape/PW and LPF/HPF.
- 1x/2x/4x/8x oversampling infrastructure; 2x default.
- Post-sum analog-distortion candidate.
- Standard Slop and Vintage variation modes.
- Global serial FX A -> B subsystem from v0.18.

## Stored sequencer layout

The raw program payload contains the 64-step / six-track sequence at bytes `128..895`:

```text
track 1: notes 128..191, velocity/flags 192..255
track 2: notes 256..319, velocity/flags 320..383
...
track 6: notes 768..831, velocity/flags 832..895
```

`P6Sequence` decodes all 384 cells while preserving unknown metadata. The low seven velocity bits are exposed as the current MIDI-velocity candidate; the high bit remains opaque until hardware evidence establishes its exact meaning.

## Reference-A calibration harness

The `analysis/` directory contains the reference index/comparator, patch-aware inference and suite automation. The comparison intentionally reports separate spectral, envelope, timing, crest and stereo metrics rather than a misleading single “accuracy percentage”. Reference-A is useful diagnostic evidence, but controlled dry Prophet-6 captures remain the required calibration truth for v0.19.

Representative tools:

```bash
python3 analysis/reference_index.py reference.mp3 --names data/factory_program_names_2015.txt --csv segments.csv --json segments.json
python3 analysis/compare_audio.py reference_segment.wav candidate.wav --out comparison.json
python3 analysis/compare_program.py 7 reference.mp3 candidate.wav --segments segments.csv --out comparison.json
python3 analysis/run_calibration_suite.py reference.mp3 manifest.csv out/ --renderer build/p6_render_events_cal
```

## Command-line tools

```bash
p6_inspect bank_or_patch.syx
p6_sequence_dump bank_or_patch.syx [message-index]
p6_render patch.syx output.wav [midi-note] [slop|vintage]
p6_render_events patch_or_bank.syx events.csv output.wav [message-index] [slop|vintage]
p6_render_cal patch.syx output.wav midi-note [calibration options]
p6_render_events_cal patch.syx events.csv output.wav [calibration options]
p6_sync_probe output.wav [sync diagnostic options]
p6_polymod_probe output.wav [Poly-Mod diagnostic options]
p6_sync_microscope trace.csv [cycle-level hard-sync options]
p6_timing_probe output_directory
p6_dsp_fingerprint
p6_perf_benchmark
```

## Build and test

Core only:

```bash
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

VST3 with JUCE 9.0.4 fetched automatically:

```bash
cmake -S . -B build-plugin -DP6_BUILD_PLUGIN=ON -DP6_FETCH_JUCE=ON
cmake --build build-plugin --config Release --target P6CorePlugin_VST3 -j2
ctest --test-dir build-plugin -C Release --output-on-failure
```

The release CI builds Windows and Linux VST3 targets, runs core/state/control/realtime/v0.18 FX tests, emits DSP fingerprints and a Linux performance snapshot, runs Linux ASan+UBSan, validates the plug-in with `pluginval --strictness-level 10`, and uploads validated artifacts.

## Priorities after v0.18

1. **v0.19 sound matching:** obtain controlled Prophet-6 hardware captures and use them as primary calibration evidence.
2. Calibrate oscillators/PW/mixer, hard sync, HPF/LPF, ADSRs, glide, Slop/Vintage, distortion, modulation depths and the v0.18 FX candidates.
3. Continue CPU optimisation only from profiling data and protect every material change with DSP fingerprints and Reference-A/hardware comparisons.
4. Keep later-OS FX selector encoding, exact LFO Sync divisions, CC4, swing ratio, chord-memory storage and sequence opaque semantics unguessed until stronger evidence exists.
5. Keep the Generic JUCE editor as a diagnostic fallback. Bespoke UI/preset workflow is v0.20 work.
6. The proposed reactive/character FX visuals — glowing tube, plasma discharge, vibrating springs, tape reels, optical lamp, ring-mod scope, ferrofluid and shimmer particles — are recorded in the parity matrix as **deferred UI exploration for v0.20+**, after the visual language and UI performance/accessibility contract are defined.

## Licensing

The project uses/adapts MIT-licensed implementation ideas/code from `joshjetson/phosphor`; attribution is maintained in `THIRD_PARTY_NOTICES.md`. `ShmKnd/Patina` was reviewed as an MIT-licensed topology/architecture reference for additional effects. GPL/AGPL projects are reference-only unless explicitly documented otherwise. Factory SysEx and third-party reference audio are **not redistributed** by this source package.
