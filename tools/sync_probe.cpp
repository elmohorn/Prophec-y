#include "p6/P6Voice.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
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
    const auto bytes = static_cast<std::uint32_t>(interleaved.size() * sizeof(float));
    f.write("RIFF", 4); write32(f, 36 + bytes); f.write("WAVE", 4);
    f.write("fmt ", 4); write32(f, 16); write16(f, 3); write16(f, 2);
    write32(f, sampleRate); write32(f, sampleRate * 8); write16(f, 8); write16(f, 32);
    f.write("data", 4); write32(f, bytes);
    f.write(reinterpret_cast<const char*>(interleaved.data()), static_cast<std::streamsize>(bytes));
    return static_cast<bool>(f);
}
int checkedOversampling(int v)
{
    if (v != 1 && v != 2 && v != 4 && v != 8)
        throw std::runtime_error("oversampling must be 1, 2, 4, or 8");
    return v;
}
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: p6_sync_probe <out.wav> [--note 41] [--slave-semis 28] [--master-semis 0] "
                     "[--shape1 0..1] [--shape2 0..1] [--pw1 0.04..0.96] [--pw2 0.04..0.96] "
                     "[--oversampling 1|2|4|8] [--seconds N] [--preroll N] [--seed N] "
                     "[--reset-phase 0..1] [--trigger-phase 0..1] [--correction legacy|step-slope] [--free-running]\n";
        return 2;
    }

    int note = 41;
    double slaveSemis = 28.0;
    double masterSemis = 0.0;
    double shape1 = 0.5, shape2 = 0.5;
    double pw1 = 0.5, pw2 = 0.5;
    int oversampling = 2;
    double seconds = 2.0, preroll = 0.35;
    std::uint32_t seed = 0x2660u;
    double resetPhase = 0.0, triggerPhase = 0.0;
    bool syncEnabled = true;
    p6::HardSyncCorrectionMode correctionMode = p6::HardSyncCorrectionMode::LegacyStepOnly;

    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* option) -> std::string {
            if (i + 1 >= argc) throw std::runtime_error(std::string("missing value for ") + option);
            return argv[++i];
        };
        if (a == "--free-running") syncEnabled = false;
        else if (a == "--note") note = std::clamp(std::stoi(next("--note")), 0, 127);
        else if (a == "--slave-semis") slaveSemis = std::stod(next("--slave-semis"));
        else if (a == "--master-semis") masterSemis = std::stod(next("--master-semis"));
        else if (a == "--shape1") shape1 = std::clamp(std::stod(next("--shape1")), 0.0, 1.0);
        else if (a == "--shape2") shape2 = std::clamp(std::stod(next("--shape2")), 0.0, 1.0);
        else if (a == "--pw1") pw1 = std::clamp(std::stod(next("--pw1")), 0.04, 0.96);
        else if (a == "--pw2") pw2 = std::clamp(std::stod(next("--pw2")), 0.04, 0.96);
        else if (a == "--oversampling") oversampling = checkedOversampling(std::stoi(next("--oversampling")));
        else if (a == "--seconds") seconds = std::clamp(std::stod(next("--seconds")), 0.1, 30.0);
        else if (a == "--preroll") preroll = std::clamp(std::stod(next("--preroll")), 0.0, 10.0);
        else if (a == "--seed") seed = static_cast<std::uint32_t>(std::stoul(next("--seed"), nullptr, 0));
        else if (a == "--reset-phase") resetPhase = std::stod(next("--reset-phase"));
        else if (a == "--trigger-phase") triggerPhase = std::stod(next("--trigger-phase"));
        else if (a == "--correction") {
            const auto mode = next("--correction");
            if (mode == "legacy") correctionMode = p6::HardSyncCorrectionMode::LegacyStepOnly;
            else if (mode == "step-slope") correctionMode = p6::HardSyncCorrectionMode::StepAndSlope;
            else throw std::runtime_error("--correction must be legacy or step-slope");
        }
        else throw std::runtime_error("unknown option: " + a);
    }

    constexpr int sr = 48000;
    p6::VoiceParams p;
    p.osc1.semitones = slaveSemis;
    p.osc2.semitones = masterSemis;
    p.osc1.shape = shape1;
    p.osc2.shape = shape2;
    p.osc1.pulseWidth = pw1;
    p.osc2.pulseWidth = pw2;
    p.osc1.level = 1.0;
    p.osc2.level = 0.0;
    p.subLevel = 0.0;
    p.noiseLevel = 0.0;
    p.sync = syncEnabled;
    p.syncResetPhase = resetPhase;
    p.syncTriggerPhase = triggerPhase;
    p.syncCorrectionMode = correctionMode;
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
    d.forceSync = syncEnabled;

    p6::P6Voice voice;
    voice.prepare(sr, seed, oversampling);
    voice.setParams(p);
    voice.setDiagnostics(d);
    voice.noteOn(note, 1.0);

    const auto pre = static_cast<std::size_t>(std::ceil(preroll * sr));
    for (std::size_t i = 0; i < pre; ++i) (void)voice.process();

    const auto count = static_cast<std::size_t>(std::ceil(seconds * sr));
    std::vector<float> audio;
    audio.reserve(count * 2);
    double peak = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double y = voice.process().dry;
        peak = std::max(peak, std::abs(y));
        audio.push_back(static_cast<float>(y));
        audio.push_back(static_cast<float>(y));
    }

    if (!writeFloatWav(argv[1], audio, sr)) {
        std::cerr << "failed to write WAV\n";
        return 3;
    }

    const double masterHz = 440.0 * std::pow(2.0, (static_cast<double>(note) + masterSemis - 69.0) / 12.0);
    const double slaveHz = 440.0 * std::pow(2.0, (static_cast<double>(note) + slaveSemis - 69.0) / 12.0);
    std::cout << "sync probe: note=" << note << " masterHz=" << masterHz << " slaveHz=" << slaveHz
              << " ratio=" << slaveHz / masterHz << " oversampling=" << oversampling
              << " resetPhase=" << resetPhase << " triggerPhase=" << triggerPhase
              << " correction=" << (correctionMode == p6::HardSyncCorrectionMode::StepAndSlope ? "step-slope" : "legacy")
              << " sync=" << syncEnabled << " peak=" << peak << '\n';
    return 0;
}
