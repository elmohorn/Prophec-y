#include "p6/P6Engine.h"
#include "p6/SysexStream.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

namespace {

void write16(std::ofstream& f, std::uint16_t v)
{
    const char b[2] = {static_cast<char>(v & 0xff), static_cast<char>((v >> 8) & 0xff)};
    f.write(b, 2);
}

void write32(std::ofstream& f, std::uint32_t v)
{
    const char b[4] = {static_cast<char>(v & 0xff), static_cast<char>((v >> 8) & 0xff),
                       static_cast<char>((v >> 16) & 0xff), static_cast<char>((v >> 24) & 0xff)};
    f.write(b, 4);
}

bool writeFloatWav(const std::string& path, const std::vector<float>& interleaved, int sampleRate)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(interleaved.size() * sizeof(float));
    f.write("RIFF", 4); write32(f, 36 + dataBytes); f.write("WAVE", 4);
    f.write("fmt ", 4); write32(f, 16); write16(f, 3); write16(f, 2); // IEEE float, stereo
    write32(f, sampleRate); write32(f, sampleRate * 2 * 4); write16(f, 8); write16(f, 32);
    f.write("data", 4); write32(f, dataBytes);
    f.write(reinterpret_cast<const char*>(interleaved.data()), static_cast<std::streamsize>(dataBytes));
    return static_cast<bool>(f);
}

std::vector<std::uint8_t> readBytes(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open input SysEx");
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 4) {
        std::cerr << "usage: p6_render_cal <program.syx> <output.wav> <midi-note> "
                     "[--notes 60,64,67] [--velocity 0..1] [--note-seconds N] [--release-seconds N] [--pre-roll-seconds N] "
                     "[--pw-inset N] [--pw-exponent N] [--pulse-normalization N] [--mixer-exponent N] [--sub-gain N] [--noise-gain N] [--filter-scale N] [--filter-base-hz N] [--filter-slope N] [--hp-filter-base-hz N] [--hp-filter-slope N] [--hp-filter-curve-exp N] [--hp-filter-top-hz N] [--filter-curve-exp N] [--filter-top-hz N] [--filter-knee-raw N] [--filter-high-slope N] [--lp-res-scale N] [--filter-env-octaves N] [--env-exponent N] [--amp-env-exponent N] [--filter-env-exponent N] [--env-max-seconds N] [--amp-env-max-seconds N] [--filter-env-max-seconds N] "
                     "[--pm-pitch-octaves N] [--pm-shape-range N] [--pm-pw-range N] [--pm-filter-octaves N] [--sync-reset-phase N] [--sync-trigger-phase N] [--sync-slope-correction] [--osc2-mixer-polarity +/-1] [--oversampling 1|2|4|8] [--osc-only|--osc1-only|--osc2-only|--sync-only] [--filter-bypass] [--poly-mod-bypass] [--reference-a-v07] [--reference-a-v08] [--reference-a-v09] [--vintage]\n";
        return 2;
    }

    const int note = std::clamp(std::stoi(argv[3]), 0, 127);
    std::vector<int> notes{note};
    double velocity = 1.0;
    double noteSeconds = 3.0;
    double releaseSeconds = 2.0;
    double preRollSeconds = 0.0;
    double filterScale = 1.0;
    int oversampling = 2;
    p6::VoiceDiagnostics diagnostics;
    bool isolationDiagnosticsActive = false;
    p6::MapperCalibration calibration;
    // Presets are selected in a pre-pass so explicit calibration flags override
    // them regardless of command-line order. If both are supplied, the last
    // preset selector wins.
    for (int i = 4; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--reference-a-v07") calibration = p6::referenceACandidateV07();
        else if (a == "--reference-a-v08") calibration = p6::referenceACandidateV08();
        else if (a == "--reference-a-v09") calibration = p6::referenceACandidateV09();
    }

    for (int i = 4; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* opt) -> double {
            if (i + 1 >= argc) throw std::runtime_error(std::string("missing value for ") + opt);
            return std::stod(argv[++i]);
        };
        if (a == "--pw-inset") calibration.pulseWidthEndInset = std::clamp(next("--pw-inset"), 0.0, 0.45);
        else if (a == "--pw-exponent") calibration.pulseWidthCurveExponent = std::clamp(next("--pw-exponent"), 0.1, 4.0);
        else if (a == "--pulse-normalization") calibration.pulseMeanNormalization = std::clamp(next("--pulse-normalization"), 0.0, 1.0);
        else if (a == "--mixer-exponent") calibration.mixerLevelExponent = std::clamp(next("--mixer-exponent"), 0.1, 4.0);
        else if (a == "--sub-gain") calibration.subGain = std::clamp(next("--sub-gain"), 0.0, 4.0);
        else if (a == "--noise-gain") calibration.noiseGain = std::clamp(next("--noise-gain"), 0.0, 4.0);
        else if (a == "--notes") {
            if (i + 1 >= argc) throw std::runtime_error("missing value for --notes");
            notes.clear();
            std::stringstream ss(argv[++i]);
            std::string token;
            while (std::getline(ss, token, ',')) {
                if (!token.empty()) notes.push_back(std::clamp(std::stoi(token), 0, 127));
            }
            if (notes.empty()) throw std::runtime_error("--notes requires at least one MIDI note");
        }
        else if (a == "--velocity") velocity = std::clamp(next("--velocity"), 0.0, 1.0);
        else if (a == "--note-seconds") noteSeconds = std::max(0.01, next("--note-seconds"));
        else if (a == "--release-seconds") releaseSeconds = std::max(0.0, next("--release-seconds"));
        else if (a == "--pre-roll-seconds") preRollSeconds = std::clamp(next("--pre-roll-seconds"), 0.0, 60.0);
        else if (a == "--filter-scale") filterScale = std::clamp(next("--filter-scale"), 0.05, 64.0);
        else if (a == "--filter-base-hz") calibration.filterBaseHz = std::clamp(next("--filter-base-hz"), 2.0, 2000.0);
        else if (a == "--filter-slope") calibration.filterSemitonesPerRaw = std::clamp(next("--filter-slope"), 0.05, 2.0);
        else if (a == "--hp-filter-base-hz") calibration.highPassFilterBaseHz = std::clamp(next("--hp-filter-base-hz"), 1.0, 2000.0);
        else if (a == "--hp-filter-slope") calibration.highPassFilterSemitonesPerRaw = std::clamp(next("--hp-filter-slope"), 0.05, 2.0);
        else if (a == "--hp-filter-curve-exp") calibration.highPassFilterCurveExponent = std::clamp(next("--hp-filter-curve-exp"), 0.0, 6.0);
        else if (a == "--hp-filter-top-hz") calibration.highPassFilterTopHz = std::clamp(next("--hp-filter-top-hz"), 1000.0, 100000.0);
        else if (a == "--filter-curve-exp") calibration.filterCurveExponent = std::clamp(next("--filter-curve-exp"), 0.05, 5.0);
        else if (a == "--filter-top-hz") calibration.filterTopHz = std::clamp(next("--filter-top-hz"), 1000.0, 100000.0);
        else if (a == "--filter-knee-raw") calibration.filterKneeRaw = std::clamp(next("--filter-knee-raw"), 0.0, 164.0);
        else if (a == "--filter-high-slope") calibration.filterHighSemitonesPerRaw = std::clamp(next("--filter-high-slope"), 0.0, 3.0);
        else if (a == "--lp-res-scale") calibration.lowPassResonanceScale = std::clamp(next("--lp-res-scale"), 0.0, 2.0);
        else if (a == "--filter-env-octaves") calibration.filterEnvRangeOctaves = std::clamp(next("--filter-env-octaves"), 1.0, 24.0);
        else if (a == "--pm-pitch-octaves") calibration.polyModOscPitchRangeOctaves = std::clamp(next("--pm-pitch-octaves"), 0.0, 12.0);
        else if (a == "--pm-shape-range") calibration.polyModShapeRange = std::clamp(next("--pm-shape-range"), 0.0, 2.0);
        else if (a == "--pm-pw-range") calibration.polyModPulseWidthRange = std::clamp(next("--pm-pw-range"), 0.0, 0.5);
        else if (a == "--pm-filter-octaves") calibration.polyModFilterRangeOctaves = std::clamp(next("--pm-filter-octaves"), 0.0, 12.0);
        else if (a == "--sync-reset-phase") calibration.syncResetPhase = next("--sync-reset-phase");
        else if (a == "--sync-trigger-phase") calibration.syncTriggerPhase = next("--sync-trigger-phase");
        else if (a == "--sync-slope-correction") calibration.syncSlopeCorrection = true;
        else if (a == "--osc2-mixer-polarity") calibration.osc2MixerPolarity = next("--osc2-mixer-polarity") < 0.0 ? -1.0 : 1.0;
        else if (a == "--oversampling") {
            if (i + 1 >= argc) throw std::runtime_error("missing value for --oversampling");
            oversampling = std::stoi(argv[++i]);
            if (oversampling != 1 && oversampling != 2 && oversampling != 4 && oversampling != 8)
                throw std::runtime_error("--oversampling must be 1, 2, 4, or 8");
        }
        else if (a == "--osc-only") { diagnostics.mix = p6::VoiceDiagnosticMix::OscillatorsOnly; diagnostics.bypassFilters = true; isolationDiagnosticsActive = true; }
        else if (a == "--osc1-only") { diagnostics.mix = p6::VoiceDiagnosticMix::Osc1Only; diagnostics.bypassFilters = true; isolationDiagnosticsActive = true; }
        else if (a == "--osc2-only") { diagnostics.mix = p6::VoiceDiagnosticMix::Osc2Only; diagnostics.bypassFilters = true; isolationDiagnosticsActive = true; }
        else if (a == "--sync-only") { diagnostics.mix = p6::VoiceDiagnosticMix::Osc1Only; diagnostics.bypassFilters = true; diagnostics.bypassPolyMod = true; diagnostics.forceSync = true; isolationDiagnosticsActive = true; }
        else if (a == "--filter-bypass") { diagnostics.bypassFilters = true; isolationDiagnosticsActive = true; }
        else if (a == "--poly-mod-bypass") { diagnostics.bypassPolyMod = true; isolationDiagnosticsActive = true; }
        else if (a == "--env-exponent") { double e = std::clamp(next("--env-exponent"), 0.2, 8.0); calibration.ampEnvelopeCurveExponent = e; calibration.filterEnvelopeCurveExponent = e; }
        else if (a == "--amp-env-exponent") calibration.ampEnvelopeCurveExponent = std::clamp(next("--amp-env-exponent"), 0.2, 8.0);
        else if (a == "--filter-env-exponent") calibration.filterEnvelopeCurveExponent = std::clamp(next("--filter-env-exponent"), 0.2, 8.0);
        else if (a == "--env-max-seconds") {
            const double s = std::clamp(next("--env-max-seconds"), 0.05, 60.0);
            calibration.ampAttackMaxSeconds = s;
            calibration.ampDecayMaxSeconds = s;
            calibration.ampReleaseMaxSeconds = s;
            calibration.filterAttackMaxSeconds = s;
            calibration.filterDecayMaxSeconds = s;
            calibration.filterReleaseMaxSeconds = s;
        } else if (a == "--amp-env-max-seconds") {
            const double sec = std::clamp(next("--amp-env-max-seconds"), 0.05, 60.0);
            calibration.ampAttackMaxSeconds = sec;
            calibration.ampDecayMaxSeconds = sec;
            calibration.ampReleaseMaxSeconds = sec;
        } else if (a == "--filter-env-max-seconds") {
            const double sec = std::clamp(next("--filter-env-max-seconds"), 0.05, 60.0);
            calibration.filterAttackMaxSeconds = sec;
            calibration.filterDecayMaxSeconds = sec;
            calibration.filterReleaseMaxSeconds = sec;
        } else if (a == "--reference-a-v07" || a == "--reference-a-v08" || a == "--reference-a-v09") {
            // Applied in the pre-pass above.
        } else if (a == "--vintage") calibration.slopMode = p6::SlopMode::Vintage;
        else throw std::runtime_error("unknown option: " + a);
    }
    calibration.filterBaseHz *= filterScale;

    const auto bytes = readBytes(argv[1]);
    const auto stream = p6::decodeP6SysexStream(bytes);
    if (stream.entries.empty()) {
        std::cerr << "no complete Prophet-6 Program/Edit Buffer message found\n";
        return 3;
    }

    constexpr int sr = 48000;
    p6::P6Engine engine;
    engine.prepare(sr, oversampling);
    engine.setVoiceDiagnostics(diagnostics);
    engine.loadPatch(stream.entries.front().patch, calibration);
    if (isolationDiagnosticsActive) {
        engine.setDistortion(0.0);
        engine.setPanSpread(0.0);
    }
    const auto preRollSamples = static_cast<std::size_t>(std::ceil(preRollSeconds * sr));
    for (std::size_t i = 0; i < preRollSamples; ++i) (void)engine.process();
    for (const int n : notes) engine.noteOn(n, velocity);

    const auto noteSamples = static_cast<std::size_t>(std::ceil(noteSeconds * sr));
    const auto releaseSamples = static_cast<std::size_t>(std::ceil(releaseSeconds * sr));
    std::vector<float> audio;
    audio.reserve((noteSamples + releaseSamples) * 2);
    double peak = 0.0;
    for (std::size_t i = 0; i < noteSamples + releaseSamples; ++i) {
        if (i == noteSamples) {
            for (const int n : notes) engine.noteOff(n);
        }
        const auto s = engine.process();
        peak = std::max({peak, std::abs(s.left), std::abs(s.right)});
        audio.push_back(static_cast<float>(s.left));
        audio.push_back(static_cast<float>(s.right));
    }

    if (!writeFloatWav(argv[2], audio, sr)) {
        std::cerr << "failed to write WAV\n";
        return 4;
    }
    std::cout << "Rendered '" << stream.entries.front().patch.name << "' float32; peak=" << peak
              << ", notes=" << notes.size() << ", preRoll=" << preRollSeconds
              << ", filterScale=" << filterScale
              << ", filterSlope=" << calibration.filterSemitonesPerRaw << ", hpBase=" << calibration.highPassFilterBaseHz << ", hpSlope=" << calibration.highPassFilterSemitonesPerRaw << ", hpCurveExp=" << calibration.highPassFilterCurveExponent << ", hpTop=" << calibration.highPassFilterTopHz
              << ", filterCurveExp=" << calibration.filterCurveExponent
              << ", filterTopHz=" << calibration.filterTopHz
              << ", filterKneeRaw=" << calibration.filterKneeRaw
              << ", filterHighSlope=" << calibration.filterHighSemitonesPerRaw
              << ", pwInset=" << calibration.pulseWidthEndInset << ", pwExp=" << calibration.pulseWidthCurveExponent << ", pulseNorm=" << calibration.pulseMeanNormalization << ", mixerExp=" << calibration.mixerLevelExponent << ", subGain=" << calibration.subGain << ", noiseGain=" << calibration.noiseGain
              << ", pmPitchOct=" << calibration.polyModOscPitchRangeOctaves << ", pmShape=" << calibration.polyModShapeRange << ", pmPW=" << calibration.polyModPulseWidthRange << ", pmFilterOct=" << calibration.polyModFilterRangeOctaves
              << ", syncResetPhase=" << calibration.syncResetPhase << ", syncTriggerPhase=" << calibration.syncTriggerPhase << ", syncSlopeCorrection=" << calibration.syncSlopeCorrection << ", osc2MixerPolarity=" << calibration.osc2MixerPolarity
              << ", oversampling=" << oversampling << ", filterBypass=" << diagnostics.bypassFilters << ", polyModBypass=" << diagnostics.bypassPolyMod << ", forceSync=" << diagnostics.forceSync
              << ", ampEnvExp=" << calibration.ampEnvelopeCurveExponent << ", filterEnvExp=" << calibration.filterEnvelopeCurveExponent << '\n';
    return 0;
}
