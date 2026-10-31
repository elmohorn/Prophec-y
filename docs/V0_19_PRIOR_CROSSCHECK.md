# v0.19 — Historical Reference-A cross-check of external priors

Status: **ACTIVE EVIDENCE LEDGER**

Before running new grids, v0.19 re-audits calibration sweeps already performed against Reference-A in v0.8/v0.9. This prevents an external implementation from being treated as a new hypothesis when the same law was already tested against real Prophet-6 audio.

The historical sweeps remain subject to their original caveats: web-compressed reference audio, inferred performances, unknown free-running oscillator phase and coupled filter/performance errors. A numerical minimum is therefore not automatically a hardware calibration.

## 1. Pulse-width endpoint / minimum-duty mapping

Phosphor's Prophet-6 prior distributes the complete raw PW range over approximately 4%..96% duty. In Prophec-y calibration terms this is:

```text
pulseWidthCurveExponent = 1.0
pulseWidthEndInset      = 0.04
```

The historical `pw_inset_fit.csv` sweep tested 13 inset values against a score combining the `Solid Bass` main/sub relationship and `Carpenter Square` spectral error.

| inset | score | rank |
|---:|---:|---:|
| 0.15 | **9.055711** | 1 |
| 0.13 | 9.058688 | 2 |
| 0.11 | 9.090496 | 3 |
| 0.18 | 9.110482 | 4 |
| 0.09 | 9.139887 | 5 |
| 0.22 | 9.178086 | 6 |
| 0.075 | 9.182970 | 7 |
| 0.06 | 9.228601 | 8 |
| 0.05 | 9.259392 | 9 |
| **0.04 (Phosphor)** | **9.291282** | **10** |
| 0.03 | 9.324072 | 11 |
| 0.00 | 9.437424 | 12 |
| 0.26 | 9.482147 | 13 |

The Phosphor value was about **2.6% worse** than the numerical minimum in that historical score. However, v0.8 explicitly did **not** promote 0.15: endpoint changes were modest and did not explain the larger `Solid Bass` mismatch, while oscillator phase and filter behavior were important nuisance variables.

**v0.19 interpretation:** Phosphor's 0.04 is not validated merely by being P6-specific, but the old 0.15 minimum is also not promoted. Both remain testable profiles. The next decision should use a larger/reference-weighted pulse-oriented corpus, especially `Carpenter Square` and `Poly Pulse`, with unrelated anchors as guards.

## 2. Narrow-pulse mean/level normalization

Prophec-y currently shares Phosphor's pulse-family behavior at `pulseMeanNormalization=1`: remove the pulse mean and apply the additional `1/(1+|mean|)`-style scaling family.

The historical `pulse_normalization_fit.csv` sweep tested 0.0..1.0. The aggregate score was best at 0.0 and worsened monotonically toward 1.0:

```text
normalization 0.0 : score 12.487845
normalization 1.0 : score 12.791864
relative delta    : +2.43% at 1.0
```

This is weak evidence against treating the Phosphor normalization as measured Prophet-6 behavior. But v0.8 found that completely removing the extra attenuation improved the `Solid Bass` main/sub ratio by only about 0.8 dB, far short of the apparent mismatch. It therefore retained 1.0 and rejected the temptation to use oscillator gain to compensate another block.

**v0.19 interpretation:** normalization=1 remains the stable/default behavior for now, but with **low hardware confidence**. A `pulse_normalization_zero_exploratory` profile is retained for future multi-source A/B rather than promoted.

## 3. Mixer and sub gain

v0.8 ran staged/broadband oscillator-mixer experiments over Brassed Off, Solid Bass and Carpenter Square. Optimizers could find lower numerical scores by changing sub gain, mixer exponent or noise gain, but the values were inconsistent/implausible across patches.

The critical discovery was free-running phase sensitivity: on `Solid Bass`, changing only pre-roll before note-on moved the measured main/sub relationship over roughly **-6.7 dB to +12.2 dB**, while the real Reference-A value (~+7.3 dB) already lay inside that phase-dependent range.

Consequently v0.8 rejected:

- a global sub-gain change;
- broadband mixer/noise exponents used as compensation;
- phase-reset changes introduced merely to improve one offline render.

**v0.19 interpretation:** keep the linear mixer/raw law and current sub gain unless a phase-marginalized multi-source test shows a stable improvement. External projects agreeing on a linear mixer law raise confidence modestly, but do not independently calibrate analog summing/headroom.

## 4. LPF cutoff law — real audio overrides the external prior

This is the clearest counterexample to blindly importing Phosphor constants.

The legacy/Phosphor-like coordinate family is approximately one semitone per raw cutoff step. Reference-A v0.7 found a substantially shallower candidate:

```text
filterBaseHz          = 88.9769483193
filterSemitonesPerRaw = 0.37
filterEnvRangeOctaves = 13.0
filterEnvExponent     = 3.0
filter ADR max        = 2.5 s
LP resonance scale    = 0.70
```

Historical weighted corpus score:

```text
legacy/default : 19.3346
v0.7 candidate : 12.4566   (-35.6%)
```

A more complex curved law improved the A-trust subset slightly but regressed the broader suite, so it was rejected. The simpler v0.7 candidate remains stronger acoustic evidence than software consensus, while still not being declared the literal circuit law.

## 5. Poly Mod spans

Reference-A v0.9 adopted a named candidate after cross-checking Phosphor:

```text
Osc1 pitch : 4 octaves
Osc1 shape : 1.0 morph unit
Osc1 PW    : 0.5 duty
LPF / HPF  : 7 octaves
```

The online demo did not isolate those destinations well enough to fit them independently. `PolyModster` combines hard sync, both Poly-Mod sources, several destinations and filter motion; `FM Bass` has cleaner note tracking but non-trivial effects.

**v0.19 interpretation:** this is a legitimate **external-prior-supported named candidate**, not a real-audio measurement. Keep it versioned and seek additional online one-destination demonstrations before promotion.

## 6. Hard sync

The v0.9 `Sync Stack` reference produced a useful decomposition:

- raising diagnostic LPF scale from 1.0 toward ~2.5 strongly improved brightness/harmonic-profile error;
- nevertheless the local sync harmonic pattern remained wrong: the model kept H5 well above H4 while the reference made H4 strongest;
- reset phase, trigger phase and Osc2 mixer-polarity sweeps did not produce a stable cross-metric improvement.

This is strong evidence that hard-sync residual modeling remains a genuine oscillator problem rather than something that should be hidden with global EQ/filter compensation.

## 7. Decision rule going forward

Historical Reference-A sweeps and external projects now enter one ledger:

- **agreement + real-audio improvement** → high-priority candidate;
- **external agreement, no isolating audio** → named candidate only;
- **external prior contradicted weakly/confounded by Reference-A** → keep both hypotheses, do not promote;
- **external prior contradicted strongly across multiple real-audio anchors** → demote/reject;
- **numerical Reference-A optimum that violates cross-patch/phase logic** → diagnostic only, not calibration.

The machine-readable status for the current priors lives in `calibration/external_priors.json`.
