#include "p6/P6Engine.h"
#include "p6/ReferenceIsolation.h"
#include "p6/SysexStream.h"

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
    const char b[2] = {
        static_cast<char>(v & 0xff),
        static_cast<char>((v >> 8) & 0xff)
    };
    f.write(b, 2);
}

void write32(std::ofstream& f, std::uint32_t v)
{
    const char b[4] = {
        static_cast<char>(v & 0xff),
        static_cast<char>((v >> 8) & 0xff),
        static_cast<char>((v >> 16) & 0xff),
        static_cast<char>((v >> 24) & 0xff)
    };
    f.write(b, 4);
}

bool writeFloatWav(const std::string& path,
                   const std::vector<float>& interleaved,
                   int sampleRate)
{
    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;

    const auto dataBytes = static_cast<std::uint32_t>(interleaved.size() * sizeof(float));
    f.write("RIFF", 4);
    write32(f, 36 + dataBytes);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    write32(f, 16);
    write16(f, 3); // IEEE float
    write16(f, 2); // stereo
    write32(f, static_cast<std::uint32_t>(sampleRate));
    write32(f, static_cast<std::uint32_t>(sampleRate * 2 * sizeof(float)));
    write16(f, static_cast<std::uint16_t>(2 * sizeof(float)));
    write16(f, 32);
    f.write("data", 4);
    write32(f, dataBytes);
    f.write(reinterpret_cast<const char*>(interleaved.data()),
            static_cast<std::streamsize>(dataBytes));
    return static_cast<bool>(f);
}

std::vector<std::uint8_t> readBytes(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("cannot open input SysEx");
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void usage()
{
    std::cout
        << "usage: p6_render_reference <program.syx> <output.wav> [options]\n"
        << "\n"
        << "Standardized v0.19 hardware-reference renderer. Output is stereo float32 WAV.\n"
        << "\n"
        << "Options:\n"
        << "  --note N                  MIDI note, default 60\n"
        << "  --velocity V              0..1, default 1\n"
        << "  --sample-rate N           48000 or 96000, default 96000\n"
        << "  --oversampling N          1, 2, 4 or 8, default 2\n"
        << "  --pre-roll-seconds N      free-running pre-roll, default 2\n"
        << "  --note-seconds N          held-note duration, default 3\n"
        << "  --release-seconds N       release capture duration, default 2\n"
        << "  --reference-a-v07         use historical Reference-A v0.7 candidate\n"
        << "  --reference-a-v08         use historical Reference-A v0.8 candidate\n"
        << "  --reference-a-v09         use historical Reference-A v0.9 candidate\n"
        << "  --dry-isolation           neutralize verified unrelated performance/global paths\n"
        << "  --osc-only                oscillators through mixer, filters bypassed; implies dry isolation\n"
        << "  --osc1-only               Osc1 only, filters bypassed; implies dry isolation\n"
        << "  --osc2-only               Osc2 only, filters bypassed; implies dry isolation\n"
        << "  --sync-only               Osc1 isolated, forced sync, Poly Mod bypassed; implies dry isolation\n"
        << "  --filter-bypass           bypass HPF/LPF only\n"
        << "  --poly-mod-bypass         bypass Poly Mod destinations only\n"
        << "  --help                    show this text\n"
        << "\n"
        << "Dry isolation edits only a temporary patch copy. It disables verified Sync, Glide,\n"
        << "Slop, velocity modulation, Pan Spread, Distortion, LFO/Aftertouch destinations,\n"
        << "digital FX, Poly Mod, Unison and Arp state while preserving Program Volume, VCA\n"
        << "envelope/filter controls and every unknown byte in the 1024-byte payload.\n";
}

int parseInt(const char* value, const char* option)
{
    try {
        return std::stoi(value);
    } catch (...) {
        throw std::runtime_error(std::string("invalid integer for ") + option);
    }
}

double parseDouble(const char* value, const char* option)
{
    try {
        return std::stod(value);
    } catch (...) {
        throw std::runtime_error(std::string("invalid number for ") + option);
    }
}

} // namespace

int main(int argc, char** argv)
{
    try {
        if (argc == 2 && std::string(argv[1]) == "--help") {
            usage();
            return 0;
        }
        if (argc < 3) {
            usage();
            return 2;
        }

        int note = 60;
        double velocity = 1.0;
        int sampleRate = 96000;
        int oversampling = 2;
        double preRollSeconds = 2.0;
        double noteSeconds = 3.0;
        double releaseSeconds = 2.0;
        p6::MapperCalibration calibration;
        p6::VoiceDiagnostics diagnostics;
        bool dryIsolation = false;

        // Calibration selectors are applied first so command order cannot make
        // a historical preset unexpectedly overwrite diagnostic settings.
        for (int i = 3; i < argc; ++i) {
            const std::string a = argv[i];
            if (a == "--reference-a-v07")
                calibration = p6::referenceACandidateV07();
            else if (a == "--reference-a-v08")
                calibration = p6::referenceACandidateV08();
            else if (a == "--reference-a-v09")
                calibration = p6::referenceACandidateV09();
        }

        for (int i = 3; i < argc; ++i) {
            const std::string a = argv[i];
            auto requireValue = [&](const char* option) -> const char* {
                if (i + 1 >= argc)
                    throw std::runtime_error(std::string("missing value for ") + option);
                return argv[++i];
            };

            if (a == "--note") {
                note = std::clamp(parseInt(requireValue("--note"), "--note"), 0, 127);
            } else if (a == "--velocity") {
                velocity = std::clamp(parseDouble(requireValue("--velocity"), "--velocity"), 0.0, 1.0);
            } else if (a == "--sample-rate") {
                sampleRate = parseInt(requireValue("--sample-rate"), "--sample-rate");
                if (sampleRate != 48000 && sampleRate != 96000)
                    throw std::runtime_error("--sample-rate must be 48000 or 96000");
            } else if (a == "--oversampling") {
                oversampling = parseInt(requireValue("--oversampling"), "--oversampling");
                if (oversampling != 1 && oversampling != 2 && oversampling != 4 && oversampling != 8)
                    throw std::runtime_error("--oversampling must be 1, 2, 4 or 8");
            } else if (a == "--pre-roll-seconds") {
                preRollSeconds = std::clamp(
                    parseDouble(requireValue("--pre-roll-seconds"), "--pre-roll-seconds"), 0.0, 60.0);
            } else if (a == "--note-seconds") {
                noteSeconds = std::max(
                    0.01, parseDouble(requireValue("--note-seconds"), "--note-seconds"));
            } else if (a == "--release-seconds") {
                releaseSeconds = std::max(
                    0.0, parseDouble(requireValue("--release-seconds"), "--release-seconds"));
            } else if (a == "--dry-isolation") {
                dryIsolation = true;
            } else if (a == "--osc-only") {
                diagnostics.mix = p6::VoiceDiagnosticMix::OscillatorsOnly;
                diagnostics.bypassFilters = true;
                dryIsolation = true;
            } else if (a == "--osc1-only") {
                diagnostics.mix = p6::VoiceDiagnosticMix::Osc1Only;
                diagnostics.bypassFilters = true;
                dryIsolation = true;
            } else if (a == "--osc2-only") {
                diagnostics.mix = p6::VoiceDiagnosticMix::Osc2Only;
                diagnostics.bypassFilters = true;
                dryIsolation = true;
            } else if (a == "--sync-only") {
                diagnostics.mix = p6::VoiceDiagnosticMix::Osc1Only;
                diagnostics.bypassFilters = true;
                diagnostics.bypassPolyMod = true;
                diagnostics.forceSync = true;
                dryIsolation = true;
            } else if (a == "--filter-bypass") {
                diagnostics.bypassFilters = true;
            } else if (a == "--poly-mod-bypass") {
                diagnostics.bypassPolyMod = true;
            } else if (a == "--reference-a-v07" || a == "--reference-a-v08" ||
                       a == "--reference-a-v09") {
                // Applied in the pre-pass above.
            } else if (a == "--help") {
                usage();
                return 0;
            } else {
                throw std::runtime_error("unknown option: " + a);
            }
        }

        const auto bytes = readBytes(argv[1]);
        const auto stream = p6::decodeP6SysexStream(bytes);
        if (stream.entries.empty()) {
            std::cerr << "no complete Prophet-6 Program/Edit Buffer message found\n";
            for (const auto& error : stream.errors)
                std::cerr << error << '\n';
            return 3;
        }

        auto patch = stream.entries.front().patch;
        if (dryIsolation)
            patch = p6::makeReferenceIsolationPatch(patch);

        p6::P6Engine engine;
        engine.prepare(static_cast<double>(sampleRate), oversampling);
        engine.setVoiceDiagnostics(diagnostics);
        engine.loadPatch(patch, calibration);

        const auto preRollSamples = static_cast<std::size_t>(
            std::ceil(preRollSeconds * static_cast<double>(sampleRate)));
        const auto noteSamples = static_cast<std::size_t>(
            std::ceil(noteSeconds * static_cast<double>(sampleRate)));
        const auto releaseSamples = static_cast<std::size_t>(
            std::ceil(releaseSeconds * static_cast<double>(sampleRate)));

        for (std::size_t i = 0; i < preRollSamples; ++i)
            static_cast<void>(engine.process());

        engine.noteOn(note, velocity);

        std::vector<float> audio;
        audio.reserve((noteSamples + releaseSamples) * 2u);
        double peak = 0.0;
        for (std::size_t i = 0; i < noteSamples + releaseSamples; ++i) {
            if (i == noteSamples)
                engine.noteOff(note);

            const auto sample = engine.process();
            peak = std::max({peak, std::abs(sample.left), std::abs(sample.right)});
            audio.push_back(static_cast<float>(sample.left));
            audio.push_back(static_cast<float>(sample.right));
        }

        if (!writeFloatWav(argv[2], audio, sampleRate)) {
            std::cerr << "failed to write WAV\n";
            return 4;
        }

        std::cout << "Rendered hardware-reference candidate '" << patch.name << "'"
                  << "; sampleRate=" << sampleRate
                  << "; float32 stereo"
                  << "; note=" << note
                  << "; velocity=" << velocity
                  << "; preRoll=" << preRollSeconds
                  << "; noteSeconds=" << noteSeconds
                  << "; releaseSeconds=" << releaseSeconds
                  << "; oversampling=" << oversampling
                  << "; dryIsolation=" << dryIsolation
                  << "; filterBypass=" << diagnostics.bypassFilters
                  << "; polyModBypass=" << diagnostics.bypassPolyMod
                  << "; forceSync=" << diagnostics.forceSync
                  << "; peak=" << peak << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "p6_render_reference: " << e.what() << '\n';
        return 5;
    }
}
