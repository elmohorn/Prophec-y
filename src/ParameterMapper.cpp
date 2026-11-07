#include "p6/ParameterMapper.h"
#include "p6/ParameterRegistry.h"

#include <algorithm>
#include <cmath>

namespace p6 {
namespace {

std::uint16_t rawByNrpn(const P6PatchState& patch, std::uint16_t nrpn, std::uint16_t fallback = 0)
{
    for (const auto& p : patch.rawParameters)
        if (p.id == nrpn) {
            // SysEx retains bytes outside documented domains for lossless
            // roundtrips. DSP consumption must use the same limits as an
            // NRPN/host edit, without rewriting the imported state.
            if (const auto* info = findProgramParameter(nrpn))
                return std::clamp(p.raw, info->minimum, info->maximum);
            return p.raw;
        }
    return fallback;
}

double unit(std::uint16_t raw, double maximum)
{
    if (maximum <= 0.0)
        return 0.0;
    return std::clamp(static_cast<double>(raw) / maximum, 0.0, 1.0);
}

double calibratedLevel(std::uint16_t raw, double exponent)
{
    const double x = unit(raw, 127.0);
    return std::pow(x, std::max(0.05, exponent));
}

double calibratedPulseWidth(std::uint16_t raw, double exponent, double endInset)
{
    // Preserve the Prophet control's symmetric centre: raw 127/128 is square,
    // while a calibration exponent can expand or compress the travel away
    // from centre without changing the end points. The oscillator itself
    // applies the final 4..96% safety clamp.
    const double centered = (std::clamp(static_cast<double>(raw), 0.0, 255.0) - 127.5) / 127.5;
    const double curved = std::copysign(std::pow(std::abs(centered), std::max(0.05, exponent)), centered);
    const double span = 0.5 - std::clamp(endInset, 0.0, 0.45);
    return 0.5 + span * curved;
}

double keyTrack(std::uint16_t raw)
{
    if (raw == 1) return 0.5;
    if (raw >= 2) return 1.0;
    return 0.0;
}

} // namespace

double signedCentered01(std::uint16_t raw, std::uint16_t center)
{
    if (center == 0)
        return 0.0;
    const auto v = static_cast<double>(raw) - static_cast<double>(center);
    return std::clamp(v / static_cast<double>(center), -1.0, 1.0);
}

double rawEnvelopeTime(std::uint16_t raw, double maxSeconds, double exponent)
{
    constexpr double minSeconds = 0.0005;
    const double t = std::clamp(static_cast<double>(raw) / 127.0, 0.0, 1.0);
    if (raw == 0)
        return minSeconds;
    return std::max(minSeconds, std::max(maxSeconds, minSeconds) * std::pow(t, exponent));
}

double rawFilterCutoffHz(std::uint16_t raw, double sampleRate,
                         const MapperCalibration& calibration)
{
    const double control = std::clamp(static_cast<double>(raw), 0.0, 164.0);
    double hz = 0.0;
    if (calibration.filterCurveExponent > 0.0 &&
        calibration.filterTopHz > calibration.filterBaseHz) {
        // Calibration candidate: log-frequency interpolation with a shaped
        // control law. This lets the lower half of the knob open more gently
        // while preserving a genuinely open high end, unlike a single shallow
        // semitone/raw slope. No claim is made that this is the hardware law.
        const double t = std::pow(control / 164.0, calibration.filterCurveExponent);
        hz = calibration.filterBaseHz *
             std::pow(calibration.filterTopHz / calibration.filterBaseHz, t);
    } else if (calibration.filterHighSemitonesPerRaw > 0.0) {
        const double knee = std::clamp(calibration.filterKneeRaw, 0.0, 164.0);
        const double lowPart = std::min(control, knee) * calibration.filterSemitonesPerRaw;
        const double highPart = std::max(0.0, control - knee) * calibration.filterHighSemitonesPerRaw;
        hz = calibration.filterBaseHz * std::pow(2.0, (lowPart + highPart) / 12.0);
    } else {
        // Legacy candidate: exponential-in-raw semitone mapping.
        const double semitones = control * calibration.filterSemitonesPerRaw;
        hz = calibration.filterBaseHz * std::pow(2.0, semitones / 12.0);
    }
    return std::clamp(hz, 5.0, std::max(20.0, sampleRate * 0.45));
}

double mappedEnvelopeTime(std::uint16_t nrpn, std::uint16_t raw, const MapperCalibration& c)
{
    switch(nrpn) {
    case 67: return rawEnvelopeTime(raw,c.ampAttackMaxSeconds,c.ampEnvelopeCurveExponent);
    case 68: return rawEnvelopeTime(raw,c.ampDecayMaxSeconds,c.ampEnvelopeCurveExponent);
    case 70: return rawEnvelopeTime(raw,c.ampReleaseMaxSeconds,c.ampEnvelopeCurveExponent);
    case 78: return rawEnvelopeTime(raw,c.filterAttackMaxSeconds,c.filterEnvelopeCurveExponent);
    case 79: return rawEnvelopeTime(raw,c.filterDecayMaxSeconds,c.filterEnvelopeCurveExponent);
    case 81: return rawEnvelopeTime(raw,c.filterReleaseMaxSeconds,c.filterEnvelopeCurveExponent);
    default: return 0.;
    }
}

double rawGlideTime(std::uint16_t raw)
{
    const double t=std::clamp(static_cast<double>(raw)/127.,0.,1.);
    return .001*std::pow(10000.,t);
}

double mappedPulseWidth(std::uint16_t raw, const MapperCalibration& c)
{
    return std::clamp(calibratedPulseWidth(raw,c.pulseWidthCurveExponent,c.pulseWidthEndInset),.04,.96);
}

double rawLfoHz(std::uint16_t raw)
{
    constexpr double minHz = 0.03;
    constexpr double maxHz = 500.0;
    const double t = std::clamp(static_cast<double>(raw) / 254.0, 0.0, 1.0);
    return minHz * std::pow(maxHz / minHz, t);
}

MapperCalibration referenceACandidateV07()
{
    MapperCalibration c;
    // Best weighted result on the v0.7 eight-case Reference-A suite.
    // Keep the linear law for now: nonlinear/high-end alternatives improved
    // the three A-trust cases slightly but generalized worse to the B cases.
    c.filterBaseHz = 88.97694831931565;
    c.filterSemitonesPerRaw = 0.37;
    c.filterEnvRangeOctaves = 13.0;
    c.filterEnvelopeCurveExponent = 3.0;
    c.filterAttackMaxSeconds = 2.5;
    c.filterDecayMaxSeconds = 2.5;
    c.filterReleaseMaxSeconds = 2.5;
    c.lowPassResonanceScale = 0.7;
    return c;
}

MapperCalibration referenceACandidateV08()
{
    auto c = referenceACandidateV07();
    // Oscillator/PW experiments did not justify changing the v0.7 behavior:
    // keep the Phosphor-derived duty-dependent normalization for now.
    c.pulseMeanNormalization = 1.0;
    // Unlike the LPF fit, HPF raw 0 must remain near a bypass-like bass
    // corner while the top of the control still needs to reach the audible
    // upper range. This convex law is a Reference-A candidate, not a measured
    // hardware transfer function.
    c.highPassFilterBaseHz = 40.0;
    c.highPassFilterSemitonesPerRaw = 1.0;
    c.highPassFilterCurveExponent = 4.0;
    c.highPassFilterTopHz = 20000.0;
    return c;
}

MapperCalibration referenceACandidateV09()
{
    auto c = referenceACandidateV08();
    // Poly Mod destination spans cross-checked against the MIT-licensed
    // Phosphor Prophet-6 model. These are kept in the versioned Reference-A
    // candidate rather than promoted to the default until the online hardware
    // corpus has enough dry Poly-Mod examples to validate them independently.
    // Pitch was already four octaves each way in v0.8.
    c.polyModOscPitchRangeOctaves = 4.0;
    c.polyModShapeRange = 1.0;
    c.polyModPulseWidthRange = 0.5;
    c.polyModFilterRangeOctaves = 7.0;
    return c;
}

MappedPatch mapPatchToDSP(const P6PatchState& patch, const MapperCalibration& c)
{
    MappedPatch out;

    const auto osc1Coarse = rawByNrpn(patch, 0, 12);
    const auto osc2Coarse = rawByNrpn(patch, 5, 12);
    out.voice.osc1.semitones = static_cast<double>(osc1Coarse) - c.oscCoarseCenterRaw;
    out.voice.osc2.semitones = static_cast<double>(osc2Coarse) - c.oscCoarseCenterRaw;
    out.voice.osc2.fineCents = signedCentered01(rawByNrpn(patch, 6, 127)) * c.osc2FineRangeCents;

    // Official shape NRPN domain is 0..254: 0=triangle, midpoint=saw, 254=pulse.
    out.voice.osc1.shape = unit(rawByNrpn(patch, 3, 127), 254.0);
    out.voice.osc2.shape = unit(rawByNrpn(patch, 8, 127), 254.0);
    out.voice.osc1.pulseWidth = calibratedPulseWidth(rawByNrpn(patch, 4, 127), c.pulseWidthCurveExponent, c.pulseWidthEndInset);
    out.voice.osc2.pulseWidth = calibratedPulseWidth(rawByNrpn(patch, 9, 127), c.pulseWidthCurveExponent, c.pulseWidthEndInset);
    out.voice.osc1.level = calibratedLevel(rawByNrpn(patch, 2, 0), c.mixerLevelExponent);
    out.voice.osc1.meanNormalizationAmount = std::clamp(c.pulseMeanNormalization, 0.0, 1.0);
    out.voice.osc2.level = calibratedLevel(rawByNrpn(patch, 7, 0), c.mixerLevelExponent);
    out.voice.osc2.meanNormalizationAmount = std::clamp(c.pulseMeanNormalization, 0.0, 1.0);
    out.voice.subLevel = calibratedLevel(rawByNrpn(patch, 27, 0), c.mixerLevelExponent) * c.subGain;
    out.voice.noiseLevel = calibratedLevel(rawByNrpn(patch, 32, 0), c.mixerLevelExponent) * c.noiseGain;
    out.voice.sync = rawByNrpn(patch, 1, 0) != 0;
    out.voice.syncResetPhase = c.syncResetPhase;
    out.voice.syncTriggerPhase = c.syncTriggerPhase;
    out.voice.syncCorrectionMode = c.syncSlopeCorrection
        ? HardSyncCorrectionMode::StepAndSlope
        : HardSyncCorrectionMode::LegacyStepOnly;
    out.voice.osc2MixerPolarity = c.osc2MixerPolarity;
    out.voice.osc2LowFrequency = rawByNrpn(patch, 10, 0) != 0;
    out.voice.osc2KeyTrack = rawByNrpn(patch, 11, 1) != 0;
    out.voice.glideMode = static_cast<std::uint8_t>(std::min<std::uint16_t>(rawByNrpn(patch, 28, 0), 3));
    out.voice.glideOn = rawByNrpn(patch, 29, 0) != 0;
    {
        out.voice.glideSeconds = rawGlideTime(rawByNrpn(patch, 30, 0)); // 1 ms .. 10 s, calibration candidate
    }

    out.voice.ampEnvAmount = unit(rawByNrpn(patch, 66, 127), 127.0);
    out.voice.ampVelocity = rawByNrpn(patch, 71, 0) != 0;
    out.voice.ampAttack = mappedEnvelopeTime(67, rawByNrpn(patch, 67, 0), c);
    out.voice.ampDecay = mappedEnvelopeTime(68, rawByNrpn(patch, 68, 0), c);
    out.voice.ampSustain = unit(rawByNrpn(patch, 69, 127), 127.0);
    out.voice.ampRelease = mappedEnvelopeTime(70, rawByNrpn(patch, 70, 0), c);

    out.voice.filterAttack = mappedEnvelopeTime(78, rawByNrpn(patch, 78, 0), c);
    out.voice.filterDecay = mappedEnvelopeTime(79, rawByNrpn(patch, 79, 0), c);
    out.voice.filterSustain = unit(rawByNrpn(patch, 80, 0), 127.0);
    out.voice.filterRelease = mappedEnvelopeTime(81, rawByNrpn(patch, 81, 0), c);

    out.voice.lowPass.cutoffControl = rawByNrpn(patch, 45, 164);
    out.voice.lowPass.resonance = std::clamp(unit(rawByNrpn(patch, 46, 0), 255.0) * c.lowPassResonanceScale, 0.0, 1.0);
    out.voice.lowPass.keyTrack = keyTrack(rawByNrpn(patch, 47, 0));
    out.voice.lowPass.velocity = rawByNrpn(patch, 48, 0) != 0;
    out.voice.lowPass.envAmountOctaves = signedCentered01(rawByNrpn(patch, 77, 127)) * c.filterEnvRangeOctaves;

    out.voice.highPass.cutoffControl = rawByNrpn(patch, 49, 0);
    out.voice.highPass.resonance = std::clamp(unit(rawByNrpn(patch, 50, 0), 255.0) * c.highPassResonanceScale, 0.0, 1.0);
    out.voice.highPass.keyTrack = keyTrack(rawByNrpn(patch, 51, 0));
    out.voice.highPass.velocity = rawByNrpn(patch, 52, 0) != 0;
    out.voice.highPass.envAmountOctaves = signedCentered01(rawByNrpn(patch, 82, 127)) * c.filterEnvRangeOctaves;

    out.voice.filterBaseHz = c.filterBaseHz;
    out.voice.filterSemitonesPerRaw = c.filterSemitonesPerRaw;
    out.voice.highPassFilterBaseHz = c.highPassFilterBaseHz;
    out.voice.highPassFilterSemitonesPerRaw = c.highPassFilterSemitonesPerRaw;
    out.voice.highPassFilterCurveExponent = c.highPassFilterCurveExponent;
    out.voice.highPassFilterTopHz = c.highPassFilterTopHz;
    out.voice.filterCurveExponent = c.filterCurveExponent;
    out.voice.filterTopHz = c.filterTopHz;
    out.voice.filterKneeRaw = c.filterKneeRaw;
    out.voice.filterHighSemitonesPerRaw = c.filterHighSemitonesPerRaw;
    out.voice.polyMod.filterEnvAmount = signedCentered01(rawByNrpn(patch, 143, 127));
    out.voice.polyMod.osc2Amount = signedCentered01(rawByNrpn(patch, 144, 127));
    out.voice.polyMod.osc1FreqAmount = rawByNrpn(patch, 145, 0) ? c.polyModOscPitchRangeOctaves : 0.0;
    out.voice.polyMod.osc1ShapeAmount = rawByNrpn(patch, 146, 0) ? c.polyModShapeRange : 0.0;
    out.voice.polyMod.osc1PulseWidthAmount = rawByNrpn(patch, 147, 0) ? c.polyModPulseWidthRange : 0.0;
    out.voice.lowPass.polyModAmountOctaves = rawByNrpn(patch, 148, 0) ? c.polyModFilterRangeOctaves : 0.0;
    out.voice.highPass.polyModAmountOctaves = rawByNrpn(patch, 149, 0) ? c.polyModFilterRangeOctaves : 0.0;

    out.lfo.rateHz = rawLfoHz(rawByNrpn(patch, 88, 0));
    out.lfo.shape = static_cast<std::uint8_t>(std::min<std::uint16_t>(rawByNrpn(patch, 90, 0), 4));
    out.lfo.initialAmount = unit(rawByNrpn(patch, 89, 0), 255.0);
    out.lfo.clockSync = rawByNrpn(patch, 91, 0) != 0;
    out.lfo.noiseMode = out.lfo.shape == 4 && rawByNrpn(patch, 88, 0) >= 254;
    out.voice.lfo.osc1Freq = rawByNrpn(patch, 93, 0) != 0;
    out.voice.lfo.osc2Freq = rawByNrpn(patch, 94, 0) != 0;
    out.voice.lfo.pulseWidth = rawByNrpn(patch, 95, 0) != 0;
    out.voice.lfo.amp = rawByNrpn(patch, 96, 0) != 0;
    out.voice.lfo.lowPass = rawByNrpn(patch, 97, 0) != 0;
    out.voice.lfo.highPass = rawByNrpn(patch, 98, 0) != 0;

    out.voice.aftertouch.amount = signedCentered01(rawByNrpn(patch, 109, 127));
    out.voice.aftertouch.osc1Freq = rawByNrpn(patch, 110, 0) != 0;
    out.voice.aftertouch.osc2Freq = rawByNrpn(patch, 111, 0) != 0;
    out.voice.aftertouch.lowPass = rawByNrpn(patch, 112, 0) != 0;
    out.voice.aftertouch.highPass = rawByNrpn(patch, 113, 0) != 0;
    out.voice.aftertouch.amp = rawByNrpn(patch, 114, 0) != 0;
    out.voice.aftertouch.lfoAmount = rawByNrpn(patch, 115, 0) != 0;

    out.panSpread = unit(rawByNrpn(patch, 63, 0), 127.0);
    out.programVolume = unit(rawByNrpn(patch, 62, 127), 127.0);
    out.distortion = unit(rawByNrpn(patch, 64, 0), 127.0);
    out.slopAmount = unit(rawByNrpn(patch, 33, 0), 127.0);
    out.slopMode = c.slopMode;
    out.unison = rawByNrpn(patch, 156, 0) != 0;
    out.unisonMode = static_cast<std::uint8_t>(std::min<std::uint16_t>(rawByNrpn(patch, 157, 0), 6));
    out.keyMode = static_cast<std::uint8_t>(std::min<std::uint16_t>(rawByNrpn(patch, 158, 0), 5));
    // The manual body and hardware behavior cap the wheel at 12 semitones.
    // The older NRPN appendix advertises 0..24, so values above 12 are still
    // preserved losslessly in P6PatchState but map to the physical 12-semitone limit.
    out.pitchBendRangeSemitones = static_cast<double>(std::min<std::uint16_t>(rawByNrpn(patch, 31, 0), 12));
    out.chordMode = out.unisonMode >= 6;

    out.arpOn = rawByNrpn(patch, 160, 0) != 0;
    out.arpMode = static_cast<std::uint8_t>(std::min<std::uint16_t>(rawByNrpn(patch, 161, 0), 4));
    out.arpRange = static_cast<std::uint8_t>(std::min<std::uint16_t>(rawByNrpn(patch, 162, 0), 2));
    out.clockDivision = static_cast<std::uint8_t>(std::min<std::uint16_t>(rawByNrpn(patch, 163, 6), 9));
    out.clockBpm = static_cast<double>(std::clamp<std::uint16_t>(rawByNrpn(patch, 167, 120), 30, 250));

    out.seqOn = rawByNrpn(patch, 168, 0) != 0;
    out.seqLengthRaw = static_cast<std::uint8_t>(std::min<std::uint16_t>(rawByNrpn(patch, 169, 63), 63));
    out.seqModeRaw = static_cast<std::uint8_t>(std::min<std::uint16_t>(rawByNrpn(patch, 170, 0), 1));
    out.seqPlayModeRaw = static_cast<std::uint8_t>(std::min<std::uint16_t>(rawByNrpn(patch, 171, 0), 1));

    return out;
}

} // namespace p6

