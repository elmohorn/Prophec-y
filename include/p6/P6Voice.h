#pragma once

#include "p6/ADSREnvelope.h"
#include "p6/AnalogSlop.h"
#include "p6/CandidateFilters.h"
#include "p6/PolyBLEPOscillator.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace p6 {

struct VoiceOscillatorParams {
    double semitones{};
    double fineCents{};
    double shape{0.5};
    double pulseWidth{0.5};
    double level{0.5};
    double meanNormalizationAmount{1.0};
};

struct VoicePolyModParams {
    double filterEnvAmount{};
    double osc2Amount{};
    double osc1FreqAmount{};
    double osc1ShapeAmount{};
    double osc1PulseWidthAmount{};
};

struct VoiceLfoRouting {
    bool osc1Freq{};
    bool osc2Freq{};
    bool pulseWidth{};
    bool amp{};
    bool lowPass{};
    bool highPass{};
    double pitchRangeSemitones{12.0};
    double filterRangeSemitones{60.0};
    double pulseWidthRange{0.5};
};

struct VoiceAftertouchRouting {
    double amount{};
    bool osc1Freq{};
    bool osc2Freq{};
    bool lowPass{};
    bool highPass{};
    bool amp{};
    bool lfoAmount{};
    double pitchRangeSemitones{12.0};
    double filterRangeSemitones{48.0};
};

struct VoiceFilterParams {
    double cutoffControl{};
    double resonance{};
    double keyTrack{};
    bool velocity{};
    double envAmountOctaves{};
    double polyModAmountOctaves{};
};

struct VoiceParams {
    VoiceOscillatorParams osc1{};
    VoiceOscillatorParams osc2{};
    VoicePolyModParams polyMod{};
    VoiceLfoRouting lfo{};
    VoiceAftertouchRouting aftertouch{};
    VoiceFilterParams lowPass{164.0, 0.0, 0.0, false, 0.0, 0.0};
    VoiceFilterParams highPass{0.0, 0.0, 0.0, false, 0.0, 0.0};
    double filterBaseHz{8.1757989156};
    double filterSemitonesPerRaw{1.0};
    double highPassFilterBaseHz{-1.0};
    double highPassFilterSemitonesPerRaw{-1.0};
    double highPassFilterCurveExponent{0.0};
    double highPassFilterTopHz{20000.0};
    double filterCurveExponent{0.0};
    double filterTopHz{20000.0};
    double filterKneeRaw{100.0};
    double filterHighSemitonesPerRaw{0.0};
    double filterDrive{1.0};
    double subLevel{};
    double noiseLevel{};
    bool sync{};
    double syncResetPhase{0.0};
    double syncTriggerPhase{0.0};
    HardSyncCorrectionMode syncCorrectionMode{HardSyncCorrectionMode::LegacyStepOnly};
    double osc2MixerPolarity{1.0};
    bool osc2LowFrequency{};
    bool osc2KeyTrack{true};
    bool glideOn{};
    std::uint8_t glideMode{};
    double glideSeconds{0.001};
    double slopDepthCents{};
    double voiceGain{1.0};
    double ampEnvAmount{1.0};
    bool ampVelocity{};
    double ampAttack{0.01};
    double ampDecay{0.2};
    double ampSustain{0.8};
    double ampRelease{0.4};
    double filterAttack{0.01};
    double filterDecay{0.2};
    double filterSustain{0.0};
    double filterRelease{0.4};
};

enum class VoiceDiagnosticMix : std::uint8_t {
    Full,
    OscillatorsOnly,
    Osc1Only,
    Osc2Only
};

struct VoiceDiagnostics {
    VoiceDiagnosticMix mix{VoiceDiagnosticMix::Full};
    bool bypassFilters{};
    bool bypassPolyMod{};
    bool forceSync{};
};

struct VoiceSample {
    double dry{};
    double preFilter{};
    double highPass{};
    double lowPass{};
    double filterEnvelope{};
    double ampEnvelope{};
    double osc2{};
    double polyModSource{};
    double slop1Cents{};
    double slop2Cents{};
};

// Per-voice analog path. Oscillators and filter state continue to free-run while
// the VCA is silent; performance shortcuts therefore must not skip processInternal().
class P6Voice {
public:
    void prepare(double sampleRate, std::uint32_t seed = 1, int oversampleFactor = 2);
    void setParams(const VoiceParams& params);
    void setDiagnostics(const VoiceDiagnostics& diagnostics) { diagnostics_ = diagnostics; }
    void setPitchBendSemitones(double semitones) { pitchBendSemitones_ = semitones; }
    void noteOn(int midiNote, double velocity01 = 1.0,
                bool retriggerEnvelope = true, bool resetEnvelopeFromZero = false);
    void noteOff();
    void silence();
    void reset();
    VoiceSample process(double lfoSignal = 0.0, double pressure01 = 0.0);

    bool active() const noexcept { return active_; }
    double currentPitchNote() const noexcept { return glideNote_; }
    double effectiveBasePitchNote() const noexcept { return glideNote_ + pitchBendSemitones_; }

private:
    VoiceSample processInternal(double lfoSignal, double pressure01);
    static double midiToHz(double note);
    static double phaseFromSeed(std::uint32_t seed);
    double filterCutoffHz(const VoiceFilterParams& filter, bool highPass,
                          double env, double polySource,
                          double lfoSemitones = 0.0,
                          double pressureSemitones = 0.0) const;
    double noise();
    void configureDecimator();
    static int sanitizedOversampleFactor(int factor);

    double hostSampleRate_{48000.0};
    double sampleRate_{96000.0};
    VoiceParams params_{};
    VoiceDiagnostics diagnostics_{};
    int oversampleFactor_{2};
    PolyBLEPOscillator osc1_{};
    PolyBLEPOscillator osc2_{};
    PolyBLEPOscillator sub_{};
    AnalogSlop slop1_{};
    AnalogSlop slop2_{};
    ADSREnvelope ampEnv_{};
    ADSREnvelope filterEnv_{};
    CandidateLowPass4Pole lowPass_{};
    CandidateHighPass2Pole highPass_{};
    double baseHz_{440.0};
    double glideNote_{69.0};
    double targetNote_{69.0};
    double glideStep_{0.0};
    double pitchBendSemitones_{};
    bool pitched_{};
    double velocity_{1.0};
    int midiNote_{69};
    bool active_{};
    std::uint32_t rng_{1};
    double initialPhase1_{};
    double initialPhase2_{};
    double initialSubPhase_{};
    std::vector<double> decimatorTaps_{};
    std::vector<double> decimatorHistory_{};
    std::size_t decimatorWrite_{};
    std::size_t decimatorNonZeroSamples_{};
};

} // namespace p6
