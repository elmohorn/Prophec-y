#pragma once

#include "p6/P6PatchState.h"
#include "p6/P6Voice.h"

#include <cstdint>

namespace p6 {

enum class SlopMode : std::uint8_t { Standard, Vintage };

// Calibration constants live in one replaceable object. Values marked
// provisional are deliberately isolated so A/B measurements can refine them
// without changing the SysEx or voice architecture.
struct MapperCalibration {
    SlopMode slopMode{SlopMode::Standard}; // hardware Global, not stored in program SysEx
    double oscCoarseCenterRaw{24.0};        // factory-bank inference: raw 24 = unison
    double osc2FineRangeCents{50.0};        // documented +/- quarter-tone
    double pulseWidthCurveExponent{1.0};    // centered raw PW -> duty; 1.0 is linear
    double pulseWidthEndInset{0.0};         // calibration: raw endpoints map inward; 0 preserves legacy clamp
    double pulseMeanNormalization{1.0};      // 1=legacy peak normalization, 0=DC removal only
    double mixerLevelExponent{1.0};         // raw mixer fader -> amplitude; 1.0 is linear
    double subGain{1.0};                    // relative analog gain of triangle sub path
    double noiseGain{1.0};                  // relative analog gain of white-noise path
    double filterBaseHz{8.1757989156};       // provisional cutoff intercept
    double filterSemitonesPerRaw{1.0};      // LPF legacy candidate: raw cutoff step in semitones
    double highPassFilterBaseHz{-1.0};       // <0 inherits LPF/shared law for backwards compatibility
    double highPassFilterSemitonesPerRaw{-1.0};
    double highPassFilterCurveExponent{0.0};
    double highPassFilterTopHz{20000.0};
    // Optional nonlinear calibration law. 0 keeps the legacy exponential-in-raw law.
    // When >0, cutoff interpolates logarithmically from filterBaseHz to filterTopHz
    // using pow(raw/164, filterCurveExponent). This is calibration-only until
    // Reference A supports promoting it to the default.
    double filterCurveExponent{0.0};
    double filterTopHz{20000.0};
    // Optional piecewise semitone law. If filterHighSemitonesPerRaw > 0 and
    // no nonlinear curve is selected, the low slope is used through the knee
    // and the high slope thereafter. This keeps low/mid settings stable while
    // allowing the top of the control to become genuinely open.
    double filterKneeRaw{100.0};
    double filterHighSemitonesPerRaw{0.0};
    double lowPassResonanceScale{1.0};      // calibration hook around raw 0..255 mapping
    double highPassResonanceScale{1.0};     // calibration hook around raw 0..255 mapping
    double filterEnvRangeOctaves{10.5833333333}; // 127 semitones at full amount candidate
    double polyModOscPitchRangeOctaves{4.0}; // provisional full-scale destination depth
    double polyModShapeRange{0.5};          // legacy provisional; v0.9 candidate uses full morph travel
    double polyModPulseWidthRange{0.48};    // legacy provisional; v0.9 candidate uses 0.5
    double polyModFilterRangeOctaves{6.0};  // legacy provisional; v0.9 candidate uses 7 octaves
    double syncResetPhase{0.0};             // slave phase on Osc2 hard-sync edge; 0 preserves legacy
    double syncTriggerPhase{0.0};           // Osc2 phase that fires sync; 0=wrap (legacy)
    bool syncSlopeCorrection{false};         // v0.11 candidate: also band-limit derivative jump at reset
    double osc2MixerPolarity{1.0};          // calibration-only relative audio polarity; PM source unchanged
    double ampAttackMaxSeconds{10.0};       // bank-fit candidate; hardware timing still to measure
    double ampDecayMaxSeconds{10.0};
    double ampReleaseMaxSeconds{10.0};
    double filterAttackMaxSeconds{10.0};
    double filterDecayMaxSeconds{10.0};
    double filterReleaseMaxSeconds{10.0};
    double ampEnvelopeCurveExponent{3.5};    // factory-bank fit candidate
    double filterEnvelopeCurveExponent{3.5}; // independently calibratable; default matches amp
};


struct MappedLfo {
    double rateHz{1.0};
    std::uint8_t shape{};
    double initialAmount{};
    bool clockSync{};
    bool noiseMode{};
};

struct MappedPatch {
    VoiceParams voice{};
    MappedLfo lfo{};
    double panSpread{};
    double programVolume{1.0};
    double distortion{};
    double slopAmount{};
    SlopMode slopMode{SlopMode::Standard};
    bool unison{};
    std::uint8_t unisonMode{};
    std::uint8_t keyMode{};
    double pitchBendRangeSemitones{};
    bool chordMode{};

    // Clock/arpeggiator controls whose SysEx offsets are verified.
    bool arpOn{};
    std::uint8_t arpMode{};       // 0 Up, 1 Down, 2 Up/Down, 3 Random, 4 Assign
    std::uint8_t arpRange{};      // raw 0..2 => 1..3 octaves
    std::uint8_t clockDivision{}; // raw 0..9, Prophet CLOCK/VALUE order
    double clockBpm{120.0};

    // Sequence controls are documented NRPNs but their program-dump byte
    // offsets are not independently verified. They are therefore populated
    // only when present in P6PatchState::rawParameters (for example after an
    // NRPN edit); sequence note/velocity data itself remains in P6Sequence.
    bool seqOn{};
    std::uint8_t seqLengthRaw{63};
    std::uint8_t seqModeRaw{};
    std::uint8_t seqPlayModeRaw{};
};

// Convert hardware-domain values to DSP-domain values. This mapper never
// mutates or replaces the raw Prophet-6 state, so imports remain lossless.
MappedPatch mapPatchToDSP(const P6PatchState& patch,
                          const MapperCalibration& calibration = {});

// Provisional calibration selected against the uploaded Reference-A factory
// demo corpus. It is intentionally named/versioned and is NOT the default: the
// performances are inferred from compressed web audio rather than captured MIDI.
MapperCalibration referenceACandidateV07();
MapperCalibration referenceACandidateV08();
MapperCalibration referenceACandidateV09();

// Small helpers exposed for deterministic tests and later calibration tools.
double rawEnvelopeTime(std::uint16_t raw, double maxSeconds, double exponent = 3.5);
double rawFilterCutoffHz(std::uint16_t raw, double sampleRate,
                         const MapperCalibration& calibration = {});
double rawLfoHz(std::uint16_t raw);
double signedCentered01(std::uint16_t raw, std::uint16_t center = 127);

} // namespace p6
