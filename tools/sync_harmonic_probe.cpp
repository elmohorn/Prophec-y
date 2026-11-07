#include "p6/P6Voice.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <string_view>
#include <vector>

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kHarmonics = 12;

double midiHz(double note)
{
    return 440.0 * std::pow(2.0, (note - 69.0) / 12.0);
}

struct Harmonics {
    std::array<double, kHarmonics> db{};
};

Harmonics measure(const std::vector<double>& x, double fundamental)
{
    std::array<double, kHarmonics> amplitude{};
    const double nMinusOne = std::max(1.0, static_cast<double>(x.size() - 1));

    for (int h = 1; h <= kHarmonics; ++h) {
        std::complex<double> sum{};
        double windowSum = 0.0;
        const double omega = 2.0 * std::numbers::pi * fundamental * static_cast<double>(h) / kSampleRate;
        for (std::size_t n = 0; n < x.size(); ++n) {
            const double w = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(n) / nMinusOne);
            const double phase = -omega * static_cast<double>(n);
            sum += x[n] * w * std::complex<double>(std::cos(phase), std::sin(phase));
            windowSum += w;
        }
        amplitude[static_cast<std::size_t>(h - 1)] = windowSum > 0.0 ? 2.0 * std::abs(sum) / windowSum : 0.0;
    }

    const double peak = *std::max_element(amplitude.begin(), amplitude.end());
    Harmonics result;
    for (std::size_t i = 0; i < amplitude.size(); ++i) {
        const double ratio = peak > 0.0 ? amplitude[i] / peak : 0.0;
        result.db[i] = 20.0 * std::log10(std::max(ratio, 1.0e-12));
    }
    return result;
}

Harmonics render(p6::HardSyncCorrectionMode mode,
                 double slaveSemis,
                 double slaveShape,
                 double pulseWidth,
                 int oversampling)
{
    constexpr int note = 41;
    constexpr double masterSemis = 0.0;
    constexpr double preRollSeconds = 0.5;
    constexpr double measureSeconds = 1.5;

    p6::VoiceParams p;
    p.osc1.semitones = slaveSemis;
    p.osc2.semitones = masterSemis;
    p.osc1.shape = slaveShape;
    p.osc2.shape = 0.5;
    p.osc1.pulseWidth = pulseWidth;
    p.osc2.pulseWidth = 0.5;
    p.osc1.level = 1.0;
    p.osc2.level = 0.0;
    p.subLevel = 0.0;
    p.noiseLevel = 0.0;
    p.sync = true;
    p.syncResetPhase = 0.0;
    p.syncTriggerPhase = 0.0;
    p.syncCorrectionMode = mode;
    p.ampAttack = 0.0;
    p.ampDecay = 0.001;
    p.ampSustain = 1.0;
    p.ampRelease = 0.001;
    p.filterAttack = 0.0;
    p.filterDecay = 0.001;
    p.filterSustain = 0.0;
    p.filterRelease = 0.001;

    p6::VoiceDiagnostics d;
    d.mix = p6::VoiceDiagnosticMix::Osc1Only;
    d.bypassFilters = true;
    d.bypassPolyMod = true;
    d.forceSync = true;

    p6::P6Voice voice;
    voice.prepare(kSampleRate, 0x2660u, oversampling);
    voice.setParams(p);
    voice.setDiagnostics(d);
    voice.noteOn(note, 1.0);

    const auto pre = static_cast<std::size_t>(std::llround(preRollSeconds * kSampleRate));
    for (std::size_t i = 0; i < pre; ++i)
        static_cast<void>(voice.process());

    const auto count = static_cast<std::size_t>(std::llround(measureSeconds * kSampleRate));
    std::vector<double> audio;
    audio.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        audio.push_back(voice.process().dry);

    return measure(audio, midiHz(note + masterSemis));
}

const char* modeName(p6::HardSyncCorrectionMode mode)
{
    return mode == p6::HardSyncCorrectionMode::StepAndSlope ? "step-slope" : "legacy";
}

void printRow(p6::HardSyncCorrectionMode mode,
              double slaveSemis,
              double shape,
              double pulseWidth,
              int oversampling,
              const Harmonics& h)
{
    std::cout << modeName(mode) << ',' << slaveSemis << ',' << shape << ',' << pulseWidth << ',' << oversampling;
    for (const double value : h.db)
        std::cout << ',' << value;
    std::cout << ',' << (h.db[3] - h.db[4]) << '\n';
}

} // namespace

int main()
{
    // +28 semitones is the critical Sync Stack diagnostic from the v0.9
    // Reference-A investigation. Nearby ratios are included so a change that
    // merely overfits the 5:1 neighborhood is obvious in the log.
    constexpr std::array<double, 6> slaveSemitoneOffsets{19.0, 24.0, 28.0, 31.0, 36.0, 40.0};
    constexpr std::array<double, 3> shapes{0.25, 0.5, 0.75};
    constexpr int oversampling = 8;

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "mode,slave_semitones,shape,pulse_width,oversampling";
    for (int h = 1; h <= kHarmonics; ++h)
        std::cout << ",h" << h << "_db";
    std::cout << ",h4_minus_h5_db\n";

    for (const double semis : slaveSemitoneOffsets) {
        for (const double shape : shapes) {
            for (const auto mode : {p6::HardSyncCorrectionMode::LegacyStepOnly,
                                    p6::HardSyncCorrectionMode::StepAndSlope}) {
                const auto h = render(mode, semis, shape, 0.5, oversampling);
                printRow(mode, semis, shape, 0.5, oversampling, h);
            }
        }
    }

    return 0;
}
