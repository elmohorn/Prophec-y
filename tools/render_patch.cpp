#include "p6/P6Engine.h"
#include "p6/SysexCodec.h"
#include "p6/SysexStream.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
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

bool writeWav(const std::string& path, const std::vector<float>& interleaved, int sampleRate)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(interleaved.size() * sizeof(std::int16_t));
    f.write("RIFF", 4); write32(f, 36 + dataBytes); f.write("WAVE", 4);
    f.write("fmt ", 4); write32(f, 16); write16(f, 1); write16(f, 2);
    write32(f, sampleRate); write32(f, sampleRate * 2 * 2); write16(f, 4); write16(f, 16);
    f.write("data", 4); write32(f, dataBytes);
    for (float x : interleaved) {
        const float c = std::clamp(x, -1.0f, 1.0f);
        const auto s = static_cast<std::int16_t>(c * 32767.0f);
        write16(f, static_cast<std::uint16_t>(s));
    }
    return static_cast<bool>(f);
}


} // namespace

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "usage: p6_render <program.syx> <output.wav> [midi-note] [slop|vintage]\n";
        return 2;
    }
    const int note = argc >= 4 ? std::clamp(std::stoi(argv[3]), 0, 127) : 60;
    p6::MapperCalibration calibration;
    if (argc >= 5 && std::string(argv[4]) == "vintage")
        calibration.slopMode = p6::SlopMode::Vintage;
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) {
        std::cerr << "cannot open input SysEx\n";
        return 3;
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
    const auto stream = p6::decodeP6SysexStream(bytes);
    if (stream.entries.empty()) {
        std::cerr << "no complete Prophet-6 Program/Edit Buffer message found\n";
        for (const auto& e : stream.errors) std::cerr << e << "\n";
        return 4;
    }
    const auto& patch = stream.entries.front().patch;

    constexpr int sr = 48000;
    p6::P6Engine engine;
    engine.prepare(sr);
    engine.loadPatch(patch, calibration);
    engine.noteOn(note, 1.0);

    constexpr int noteSamples = sr * 3;
    constexpr int releaseSamples = sr * 2;
    std::vector<float> audio;
    audio.reserve((noteSamples + releaseSamples) * 2);
    for (int i = 0; i < noteSamples + releaseSamples; ++i) {
        if (i == noteSamples)
            engine.noteOff(note);
        const auto s = engine.process();
        audio.push_back(static_cast<float>(s.left));
        audio.push_back(static_cast<float>(s.right));
    }
    if (!writeWav(argv[2], audio, sr)) {
        std::cerr << "failed to write WAV\n";
        return 6;
    }
    std::cout << "Rendered '" << patch.name << "' to " << argv[2]
              << " (candidate DSP, not yet hardware-calibrated)\n";
    return 0;
}
