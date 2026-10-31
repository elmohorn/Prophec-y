# v0.19 calibration research review — 31 October 2026

This is a targeted engineering review, not a systematic-review claim. Searches covered Prophet-6/Prophet-family emulators, permissive DSP libraries, oscillator synchronization, BLEP/BLAMP, nonlinear antialiasing, filter identification, envelope laws and synthesis loss functions. Primary repositories, author papers, conference archives and manufacturer documentation were used for substantive conclusions. Search snippets and third-party descriptions were discovery aids only.

## Decision

Continue with real Prophet-6 Reference-A as the acoustic anchor. Use external implementations to generate **separate candidates** and literature to design measurements and numerical oracles. Do not replace the current mapper with a consensus of emulator constants. No external source code was incorporated in this research round.

## Projects reviewed

Audit depth is explicit: a README/algorithm reference is not an end-to-end source audit or an independent hardware validation.

| Project / primary source | License and audit depth | What it contributes | Transfer boundary / action |
|---|---|---|---|
| [Phosphor](https://github.com/joshjetson/phosphor/blob/33b1073fa1b56315d0ca877b0210efec40f68f4d/crates/phosphor-dsp/src/prophet6.rs) | MIT; direct P6 engine, parameter-law comments and source inspected at `33b1073f` | Closest state-aware external P6 implementation; PW, morph, Poly Mod and state hypotheses | Cutoff, ADSR and LFO comments explicitly identify judgment-based laws. Many reported measurements are tests of **its own software**, not recordings of hardware. Shared ancestry with Prophec-y is not independent confirmation. |
| [Patina](https://github.com/ShmKnd/Patina) | MIT; project architecture/README reviewed; prior FX audit retained | BBD/compander, OTA, drive and modulation building blocks | Generic modeling prior; contains no demonstrated P6 raw-control-to-physical calibration. Do not import its constants as Prophet constants. |
| [Melodee PROPHET](https://github.com/keremimo/melodee/blob/main/docs/PROPHET5_PROTOTYPE.md) | GPL; related-instrument documentation reviewed; existing prior matrix retained | Reports fitting Prophet-5 Rev4 envelopes, tracking, vibrato and LFO against recordings | Useful alternative curve families; different instrument, state layout and filter modes. Do not copy code or carry fitted P5 coefficients into P6. |
| [AnalogSynth](https://github.com/straplocked/analog-synth) | GPL; existing source audit and repository identity checked | Contrasting P6-inspired general VA architecture | Previously audited reversed sync topology is incompatible with P6. Inspiration is not calibration evidence; no acoustic vote. |
| [Prop-5](https://github.com/yamatech/Prop-5/tree/cb560bf109517c7e5a6322213a9e96ed92d8f818) | GPLv3; README and DSP file inventory reviewed | JUCE Prophet-family instrument and product reference | Different target and no quantitative P6 calibration established by inspected material; reference only. |
| [chowdsp_wdf](https://github.com/Chowdhury-DSP/chowdsp_wdf/tree/43bcd295e8403de913f9966f4fd7ff9d17c9f1a5) | BSD-3-Clause; README/license inspected | Real-time circuit-model infrastructure, nonlinear elements and optional SIMD | Useful if circuit topology becomes sufficiently constrained. WDF alone cannot identify unknown P6 components or control laws. No new dependency introduced. |
| [Signalsmith DSP](https://github.com/Signalsmith-Audio/dsp/tree/ca390d28c41ea5108a528e2e81faa4088f2f8ac8) | MIT; README/license and module inventory inspected | Delay/interpolation, FFT/STFT, filters and measured numerical testing approach | Infrastructure reference; not a P6 model. Keep current DSP until profiling or acoustic evidence justifies a replacement. |
| [auraloss](https://github.com/csteinmetz1/auraloss/tree/0853e9e732cd32a7b23f11ef4f0d2975d259e653) | Apache-2.0; README/license inspected | Multi-resolution STFT and mid/side comparison families | Methodological basis for reporting separate spectral/convergence/stereo metrics. Current comparator is independent NumPy/SciPy code, **not an auraloss-equivalent implementation**. No PyTorch dependency required. |
| [DDSP](https://github.com/magenta/ddsp) | Apache-2.0; paper/project documentation inspected | Analysis-by-synthesis with interpretable signal generators and spectral objectives | Supports structured fitting, not replacing this synth with a neural generator. A differentiable model does not solve unknown MIDI, codec or parameter identifiability. |
| [SST Filters / Surge](https://surge-synthesizer.github.io/sst-docs/docs/sst-filters/) | GPLv3; official library documentation inspected | Broad comparison family for resonant and nonlinear filter behavior | Alternative topologies for research; no source integration and no claim that a ladder/OB-Xd/K35 is the Prophet-6 filter. |

## Literature and manufacturer evidence

The proposed experiments below are our engineering inferences from these sources. The sources do not publish a complete measured Prophet-6 calibration.

| Primary source | Finding relevant to this work | Specific use / limitation |
|---|---|---|
| [Sequential, Prophet-6 Operation Manual](https://www.sequential.com/wp-content/uploads/2019/03/Prophet-6-Operation-Manual.pdf) and [product specification](https://sequential.com/modern-analog/prophet-6/) | Establish voice structure, documented controls and Osc2-master/Osc1-slave direction | Constrain state/topology before fitting sound; do not extrapolate undocumented cutoff/ADSR/LFO physical constants. |
| [Sequential, A Brief Explanation on Calibration](https://support.sequential.com/hc/en-gb/articles/13687984487186-A-Brief-Explanation-on-Calibration) (2023) | VCO instruments use temperature-indexed oscillator/filter calibration tables | Expect real-unit drift and tuning variation. Fit broad spectral/behavioral laws across examples, not every cent of one take. |
| [Sound Semiconductor, SSI2140 datasheet, rev. 3.1.1](https://www.soundsemiconductor.com/downloads/ssi2140datasheet.pdf) (2023) | Describes an updated SSM2040-family core, configurable sections, input overdrive and Q compensation | Helps formulate filter/saturation alternatives. **Does not establish that a P6 contains this chip or this exact reference circuit.** |
| [Brandt E. Hard Sync Without Aliasing](https://www.cs.cmu.edu/~eli/papers/icmc01-hardsync.pdf) (ICMC 2001) | Describes bandlimited discontinuity treatment and minimum-phase step correction for hard sync | Build/check an independent static sync spectrum before blaming filters. Master periodicity may differ from the strongest partial. |
| [Timoney J, Lazzarini V, Kleimola J, Pekonen J, Välimäki V. Virtual Analog Oscillator Hard Synchronisation: Fourier series and an efficient implementation](https://www.dafx.de/paper-archive/2012/papers/dafx12_submission_37.pdf) (DAFx 2012) | Develops a Fourier/filter-based alternative with comparison to analog measurements | Candidate numerical oracle and alternative architecture. Their hardware measurements do not provide the P6-specific reset/morph law. |
| [Esqueda F, Välimäki V, Bilbao S. Rounding Corners with BLAMP](https://www.dafx.de/paper-archive/2016/dafxpapers/18-DAFx-16_paper_33-PN.pdf) (DAFx 2016) | Treats first-derivative discontinuities using polynomial BLAMP | Test existing step-only versus step+slope hooks, especially triangular/morph corners. Reduced aliasing does not imply correct low-order P6 harmonic amplitudes. |
| [La Pastina PP, D'Angelo S. A General Antialiasing Method for Sine Hard Sync](https://dafx.de/paper-archive/2022/papers/DAFx20in22_paper_3.pdf) (DAFx 2022) | FIR-kernel residual formulation handles sine reset discontinuities | Useful for generalized smooth-waveform sync research. Not a direct drop-in for the current trapezoidal oscillator or evidence for changing its tuning. |
| [Roth J, Keller D, Castañeda O, Studer C. Alias-Free Oscillator Synchronization via Additive Synthesis](https://www.dafx.de/paper-archive/details/D2GH311QWm4-813bMkivRw) (DAFx 2026) | Spectral resampling maps free-running Fourier coefficients to synchronized spectra | Promising **offline oracle**. The reported 512-harmonic real-time implementation uses a custom ASIC; do not infer ordinary CPU/Pi cost from that result. |
| [Holters M. Antiderivative Antialiasing for Stateful Systems](https://www.dafx.de/paper-archive/2019/DAFx2019_paper_4.pdf) (DAFx 2019) | Extends ADAA to stateful nonlinear systems and discusses added delay/response changes | Distortion/filter aliasing candidate only after profiling and response matching. Blind insertion into a feedback loop can change the intended filter. |
| [Esqueda F, Kuznetsov B, Parker JD. Differentiable White-Box Virtual Analog Modeling](https://dafx.de/paper-archive/2021/proceedings/papers/DAFx20in21_paper_39.pdf) (DAFx 2020–21) | Optimizes approximate circuit parameters against measured input/output pairs | Supports retaining interpretable model parameters. Our factory-demo outputs lack isolated circuit inputs, so the same identification guarantee does not apply. |
| [Engel J, Hantrakul L, Gu C, Roberts A. DDSP: Differentiable Digital Signal Processing](https://arxiv.org/abs/2001.04643) (2020) | Combines interpretable DSP components and learned/fitted audio models | Use multiple spectral resolutions and explicit signal priors; keep timing/performance uncertainty separate from DSP fitting. |
| [Redmon N. Envelope generators—ADSR Part 1](https://www.earlevel.com/main/2013/06/01/envelope-generators/) (2013; primary engineering tutorial) | Describes ADSR state transitions and exponential charging/discharging behavior | Compare curve shape, time constant and endpoint convention independently. This is not a peer-reviewed or P6-specific timing calibration. |

The Native Instruments `VAFilterDesign_2.1.2.pdf` URL found in other projects returned 404 in this run. The book is therefore not represented as a newly inspected full-text source, and no new design decision depends on that failed fetch.

## Experiments produced by this review

1. **Cross-project PW prior:** compare current clamp law with direct 4–96% mapping on identical patch/events/pre-roll realizations.
2. **Pulse normalization:** compare current normalization with removal of extra peak normalization; retain DC safety and report gain matching separately.
3. **Historical filter/envelope candidates:** re-render v07/v08/v09 on the current v0.19 engine rather than replaying old summary scores.
4. **Hard sync:** retain independent H4/H5, F0 and H1–H16 diagnostics; compare existing step+slope hook without changing raw oscillator semitone mapping.
5. **Metric robustness:** pair bin-wise multi-resolution distances with broad spectral-band shape, envelope, stereo and harmonic diagnostics. A single favorable score cannot promote a model.
6. **Nuisance sensitivity:** use the same four pre-rolls for all candidates; report median and range, not each candidate's best phase. Pre-roll also changes Slop state, so these are not independent hardware trials.
7. **Independent references next:** continue annotating the existing AutomaticGainsay oscillator/filter/Poly Mod/FX inventory. Video presence alone does not establish known settings or usable dry intervals.

## Promotion outcome for this round

See `V0_19_RECOVERED_AUDIO_RESULTS.md` and `calibration/v019_recovered_audio/`. Candidate registry additions are experimental renderer profiles. No acoustic mapper defaults, canonical program bytes, oscillator coarse law, filter topology or FX encoding have been changed. v0.19 remains ACTIVE.
