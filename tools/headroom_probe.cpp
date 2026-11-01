#include "p6/P6Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

namespace {

constexpr double kSampleRate = 48000.0;
constexpr double kInitLevel = 110.0 / 127.0;
constexpr std::array<int, 6> kChordIntervals{0, 4, 7, 12, 16, 19};
constexpr std::array<int, 4> kProbeRoots{36, 48, 60, 72};

struct Stats {
    double peak{};
    double rms{};
};

[[nodiscard]] double dbfs(double amplitude)
{
    return amplitude > 0.0 ? 20.0 * std::log10(amplitude)
                           : -std::numeric_limits<double>::infinity();
}

Stats renderCase(int voiceCount,
                 bool unison,
                 double osc1Level,
                 double osc2Level,
                 double programVolume,
                 bool bypassFilters,
                 int root)
{
    p6::P6Engine engine;
    engine.prepare(kSampleRate, 2);

    p6::VoiceParams voice;
    voice.osc1.level = std::clamp(osc1Level, 0.0, 1.0);
    voice.osc2.level = std::clamp(osc2Level, 0.0, 1.0);
    voice.osc1.shape = 0.5; // saw midpoint in the Prophet morph family
    voice.osc2.shape = 0.5;
    voice.osc1.pulseWidth = 0.5;
    voice.osc2.pulseWidth = 0.5;
    voice.subLevel = 0.0;
    voice.noiseLevel = 0.0;
    voice.ampEnvAmount = 1.0;
    voice.ampAttack = 0.0005;
    voice.ampDecay = 0.0005;
    voice.ampSustain = 1.0;
    voice.ampRelease = 0.05;
    voice.filterAttack = 0.0005;
    voice.filterDecay = 0.0005;
    voice.filterSustain = 0.0;
    voice.filterRelease = 0.05;
    voice.lowPass.cutoffControl = 164.0;
    voice.lowPass.resonance = 0.0;
    voice.highPass.cutoffControl = 0.0;
    voice.highPass.resonance = 0.0;
    engine.setVoiceParams(voice);

    p6::VoiceDiagnostics diagnostics;
    diagnostics.bypassFilters = bypassFilters;
    diagnostics.bypassPolyMod = true;
    engine.setVoiceDiagnostics(diagnostics);

    engine.setPanSpread(0.0);
    engine.setProgramVolume(programVolume);
    engine.setMidiVolume(1.0);
    engine.setDistortion(0.0);
    engine.setSlop(0.0, p6::SlopMode::Standard);

    const auto voices = std::clamp(voiceCount, 1, 6);
    if (unison) {
        engine.setUnison(true, 0.0, static_cast<std::size_t>(voices));
        engine.noteOn(root, 1.0);
    } else {
        engine.setUnison(false);
        for (int i = 0; i < voices; ++i)
            engine.noteOn(root + kChordIntervals[static_cast<std::size_t>(i)], 1.0);
    }

    const int warmup = static_cast<int>(kSampleRate * 0.20);
    const int measure = static_cast<int>(kSampleRate * 1.00);
    for (int i = 0; i < warmup; ++i)
        (void) engine.process();

    double peak = 0.0;
    double sumSquares = 0.0;
    for (int i = 0; i < measure; ++i) {
        const auto s = engine.process();
        peak = std::max({peak, std::abs(s.left), std::abs(s.right)});
        sumSquares += 0.5 * (s.left * s.left + s.right * s.right);
    }

    return {peak, std::sqrt(sumSquares / static_cast<double>(measure))};
}

Stats worstAcrossRoots(int voiceCount,
                       bool unison,
                       double osc1Level,
                       double osc2Level,
                       double programVolume,
                       bool bypassFilters)
{
    Stats worst{};
    for (const int root : kProbeRoots) {
        const auto s = renderCase(voiceCount, unison, osc1Level, osc2Level,
                                  programVolume, bypassFilters, root);
        if (s.peak > worst.peak)
            worst.peak = s.peak;
        if (s.rms > worst.rms)
            worst.rms = s.rms;
    }
    return worst;
}

void printTable(const std::string& title,
                double osc1Level,
                double osc2Level,
                double programVolume,
                bool bypassFilters)
{
    std::cout << "\n" << title << "\n";
    std::cout << "voices  poly_peak  poly_dBFS  unison_peak  unison_dBFS\n";
    for (int voices = 1; voices <= 6; ++voices) {
        const auto poly = worstAcrossRoots(voices, false, osc1Level, osc2Level,
                                           programVolume, bypassFilters);
        const auto uni = worstAcrossRoots(voices, true, osc1Level, osc2Level,
                                          programVolume, bypassFilters);
        std::cout << std::setw(6) << voices << "  "
                  << std::fixed << std::setprecision(4)
                  << std::setw(9) << poly.peak << "  "
                  << std::setw(9) << dbfs(poly.peak) << "  "
                  << std::setw(11) << uni.peak << "  "
                  << std::setw(11) << dbfs(uni.peak) << "\n";
    }
}

} // namespace

int main()
{
    std::cout << "Prophec-Y deterministic headroom probe\n"
              << "48 kHz, 2x voice oversampling, centered pan, distortion/FX off\n"
              << "Peak = worst of roots C2/C3/C4/C5 over one sustained second.\n";

    printTable("A) Init-like gain staging, full filter path",
               kInitLevel, 0.0, kInitLevel, false);
    printTable("B) Init-like gain staging, filters bypassed",
               kInitLevel, 0.0, kInitLevel, true);
    printTable("C) Two oscillators at maximum, full filter path",
               1.0, 1.0, 1.0, false);

    std::cout << "\nInterpretation: peak > 1.0 (0 dBFS) leaves the plug-in above nominal "
                 "full scale. Float hosts can carry it, but a downstream fixed-scale stage "
                 "can clip. This probe intentionally does not apply a limiter or compensation.\n";
    return 0;
}
