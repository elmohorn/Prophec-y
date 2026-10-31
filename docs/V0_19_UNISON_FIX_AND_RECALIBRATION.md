# v0.19 — Unison release-tail fix and corrected calibration

Date: 31 October 2026. Fixed engine: `23910ba4f4136fd8ae419f6b2da9eccbae89902b`.
Status: **functional defect fixed; sound matching remains ACTIVE**.

Follow-up: [new official references and frozen holdout protocol](V0_19_HOLDOUT_AND_MEDIA_INTEGRITY.md). The complete baseline CI subsequently passed on Linux and Windows. The 12-octave profile remains an audition candidate.

## The important result is a functional fix

The Solid Bass reference continues sounding through its middle note. Our old current-profile render was exactly silent during 1–2 s. That is not a subtle cutoff, envelope-law or codec discrepancy.

Cause: in the plain Low/High/Last Unison modes, a new key after every previous key was released could inherit the old voice's **Release** stage. `P6Voice` correctly remained active during that tail, but `retargetUnison(false)` interpreted that lifetime as a continuing legato phrase and did not restart the envelopes. The new key was marked held while its amplifier continued releasing into silence.

Fix in `P6Engine`:

1. Record whether this is the first held key before registering the new note.
2. Re-attack the envelopes for a new held-key phrase even if the previous release remains audible.
3. Start that attack from the current envelope level; reset from zero only for explicit retrigger key modes.
4. Preserve existing held-key legato priority, pitch/glide semantics, program bytes and calibration defaults.

`testUnisonNewPhraseReattacksReleaseTail` covers all six key modes with zero and 20 ms gaps. It fails on the old code because the new sustained note loses its active voice, and passes after the fix. Existing low-priority/retrigger tests remain passing.

## Measured before/after on the same inputs

The factory program, inferred MIDI, pre-roll grid, sample rate, oversampling and current mapper are unchanged.

| Solid Bass metric | Before | After |
|---|---:|---:|
| Raw candidate RMS, 1–2 s, pre-roll 0 | **0** | **.6596285** |
| Median normalized envelope MAE | .401865 | .152371 |
| Median MR log-spectrum MAE, dB | 14.436986 | 13.919988 |
| Median broad-band shape RMSE, dB | 29.35586 | 27.7754 approximately |

The RMS row compares two internal renders, not unknown hardware output gain. The other rows use the existing gain-normalized reference metrics. This is restoration of a missing note, **not a percentage of audible similarity**.

In the recovered five-case suite, Solid Bass changes at all four pre-rolls; the other four references remain **bit-identical in 16/16 before/after current-profile complete WAV pairs**. Existing dry/sync/FX/1x/Pan fingerprints also stay unchanged. The old fingerprint suite did not contain the failing new-phrase case, which is why the new behavioral regression is necessary.

The preceding measurement reports now carry a historical caveat. Preserve their data as measurements of the old engine, but do not use the affected Solid Bass scores to promote DSP laws. The independent static Fourier oracle is unaffected.

## Re-test event uncertainty instead of fitting around the bug

The original inferred events use audible pitch-region durations; real key releases are not known. A sensitivity tool now generates 25/50/75/100% gate-duration hypotheses while keeping original note onsets, pitches and velocities unchanged. The hypotheses are explicitly C-trust, not recovered ground-truth MIDI.

We separately tested amp segment maximums 10/30/60 s. On this specific patch, amp attack and decay are at raw 0 and sustain is full, so this primarily tests its release behavior. No filter calibration is changed in this experiment.

**48 corrected-engine renders = four gate hypotheses × three amp-time laws × four pre-rolls.** With the original amp maximum:

| Gate fraction | Corrected envelope MAE | Corrected MR MAE, dB |
|---|---:|---:|
| 25% | .4189 | 14.6410 |
| 50% | .3051 | 14.3670 |
| 75% | .2254 | 14.1195 |
| 100% — original inferred durations | **.1524** | **13.9200** |

Before the engine fix, the 50% gate appeared to improve envelope error over the original durations. After the fix, it is substantially worse. This is a concrete example of an inferred-performance adjustment masking a runtime defect.

Increasing the amp maximum to 60 s barely changes the original-duration result (.1524 → .1521). There is no evidence here for promoting a sixfold amp-time change. The result supports retaining the original inferred events for this comparison; it does not prove the actual physical key-release times.

## Corrected local filter grid

After the fix, re-ran **360 renders**: depth 10.5833/11/11.5/12/12.5/13 octaves × LP resonance scale .7/.85/1 × five cases × four pre-rolls. Static LP/HP laws and envelope timing remain current. The pre-fix run is retained separately as historical evidence, not pooled with corrected data.

The 12-octave depth-only candidate is a better-balanced direction than adopting the old whole profile. It keeps current resonance and improves **both MR and broad-band spectral distances in all 20 matched-pre-roll pairs** across the five references.

| Reference | Current MR MAE, dB | 12-octave candidate MR MAE, dB | Envelope assessment |
|---|---:|---:|---|
| Brassed Off | 6.262 | 6.054 | essentially unchanged |
| Solid Bass | 13.920 | 13.712 | median improves by about .0125; one pre-roll worsens by .0021 |
| Carpenter Square | 11.156 | 10.764 | small worsening, about .0006 median |
| Sync Stack, diagnostic | 10.400 | 10.384 | essentially unchanged; harmonic inversion still unresolved |
| PolyModster, exploratory | 13.126 | 12.380 | generally better; one pre-roll envelope slightly worse |

See the exact values and ranges in `calibration/v019_unison_recalibration/fixed_local_compact.csv` and `fixed_local_comparisons.csv`. These numbers are diagnostics, not scores directly comparable with the historical v0.7 composite statistic.

The registry now provides `filter_env_depth12_v019` as **promising-not-promoted**. It can be auditioned with `render_external_prior_suite.py` or `compare_online_suite.py`. No filter-depth or resonance default has been changed. The candidates were selected on this same recording corpus, with no independent source holdout, so 12 octaves is not established as the exact hardware law.

## Validation and provenance

- Full Release build succeeded.
- CTest **8/8** passes, including the new twelve-scenario Unison regression.
- Existing deterministic DSP fingerprints remain unchanged.
- Python measurement/oracle/event-hypothesis tests: **11/11** pass.
- Input bank and excerpt hashes remain the previously recorded values.
- Fixed-grid provenance points at engine commit `23910ba4...`; pilot provenance explicitly says baseline plus the same Unison fix.
- No third-party audio or factory SysEx is redistributed.

The full online source was not recovered in this round: the public YouTube acquisition failed certificate verification, including the attempt using the system CA bundle. Verification was not disabled. Therefore **the corpus is still five excerpts from one source**, not an expanded or independent validation set. Work proceeded on the recovered audio and exposed the functional defect above.

## Reproduce

```bash
python3 tools/probe_event_uncertainty.py calibration/recovered_reference_suite_v019.csv \
  --case RA024 --output reference_private/event_probe_inputs
python3 tools/compare_online_suite.py reference_private/event_probe_inputs/manifest.csv \
  --profiles-file reference_private/event_probe_inputs/profiles.json \
  --profiles ampmax10,ampmax30,ampmax60 \
  --renderer build/p6_render_events_cal --source-sha 23910ba4f4136fd8ae419f6b2da9eccbae89902b \
  --output reference_private/v019_event_probe_fixed
```

For the depth/resonance grid use `calibration/filter_local_profiles_v019.json`; its 18 profile keys enumerate the local search. Keep the same four pre-rolls and five-case manifest. Derived input events, normalized-envelope samples, comparison rows and provenance are retained under `calibration/v019_unison_recalibration/`.

Next work: cross-check the 12-octave audition candidate on additional usable recordings/programs, and continue the Sync Stack source-mixture investigation. The corrected Unison behavior is a functional fix, not a calibration hypothesis.
