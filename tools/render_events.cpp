#include "p6/P6Engine.h"
#include "p6/SysexStream.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct NoteEvent {
    double time{};
    int note{};
    double velocity{};
    bool on{};
};

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
    write32(f, sampleRate); write32(f, sampleRate * 4); write16(f, 4); write16(f, 16);
    f.write("data", 4); write32(f, dataBytes);
    for (float x : interleaved) {
        const float c = std::clamp(x, -1.0f, 1.0f);
        const auto s = static_cast<std::int16_t>(std::lrint(c * 32767.0f));
        write16(f, static_cast<std::uint16_t>(s));
    }
    return static_cast<bool>(f);
}

std::vector<std::uint8_t> readBytes(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<NoteEvent> readEvents(const std::string& path)
{
    std::ifstream in(path);
    if (!in)
        throw std::runtime_error("cannot open event CSV");

    std::vector<NoteEvent> events;
    std::string line;
    std::size_t lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        if (line.empty() || line[0] == '#')
            continue;
        if (lineNo == 1 && line.find("time") != std::string::npos)
            continue;
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        double time = 0.0, velocity = 1.0, duration = 0.0;
        int note = 60;
        if (!(ss >> time >> note >> velocity >> duration))
            throw std::runtime_error("bad event CSV line " + std::to_string(lineNo));
        if (time < 0.0 || duration <= 0.0 || note < 0 || note > 127 || velocity < 0.0)
            throw std::runtime_error("out-of-range event CSV line " + std::to_string(lineNo));
        velocity = std::clamp(velocity, 0.0, 1.0);
        events.push_back({time, note, velocity, true});
        events.push_back({time + duration, note, 0.0, false});
    }
    std::stable_sort(events.begin(), events.end(), [](const auto& a, const auto& b) {
        if (a.time != b.time) return a.time < b.time;
        return !a.on && b.on; // note-off before a same-time note-on
    });
    return events;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 4 || argc > 6) {
        std::cerr << "usage: p6_render_events <program-or-bank.syx> <events.csv> <output.wav> [message-index] [slop|vintage]\n"
                     "events.csv: time_s,midi_note,velocity_0_to_1,duration_s\n";
        return 2;
    }

    const auto bytes = readBytes(argv[1]);
    const auto stream = p6::decodeP6SysexStream(bytes);
    if (stream.entries.empty()) {
        std::cerr << "no complete Prophet-6 Program/Edit Buffer message found\n";
        return 3;
    }

    std::size_t messageIndex = 0;
    if (argc >= 5) {
        try { messageIndex = static_cast<std::size_t>(std::stoul(argv[4])); }
        catch (...) { std::cerr << "invalid message-index\n"; return 2; }
    }
    if (messageIndex >= stream.entries.size()) {
        std::cerr << "message-index out of range\n";
        return 2;
    }

    p6::MapperCalibration calibration;
    if (argc >= 6 && std::string(argv[5]) == "vintage")
        calibration.slopMode = p6::SlopMode::Vintage;

    std::vector<NoteEvent> events;
    try { events = readEvents(argv[2]); }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 4; }
    if (events.empty()) {
        std::cerr << "event CSV contains no notes\n";
        return 4;
    }

    constexpr int sampleRate = 48000;
    p6::P6Engine engine;
    engine.prepare(sampleRate);
    engine.loadPatch(stream.entries[messageIndex].patch, calibration);

    const double lastEvent = events.back().time;
    const double totalSeconds = lastEvent + 2.0;
    const auto totalSamples = static_cast<std::size_t>(std::ceil(totalSeconds * sampleRate));
    std::vector<float> audio;
    audio.reserve(totalSamples * 2);

    std::size_t nextEvent = 0;
    for (std::size_t i = 0; i < totalSamples; ++i) {
        const double now = static_cast<double>(i) / sampleRate;
        while (nextEvent < events.size() && events[nextEvent].time <= now + 0.5 / sampleRate) {
            const auto& e = events[nextEvent++];
            if (e.on)
                engine.noteOn(e.note, e.velocity);
            else
                engine.noteOff(e.note);
        }
        const auto s = engine.process();
        audio.push_back(static_cast<float>(s.left));
        audio.push_back(static_cast<float>(s.right));
    }

    if (!writeWav(argv[3], audio, sampleRate)) {
        std::cerr << "failed to write WAV\n";
        return 5;
    }
    const auto& patch = stream.entries[messageIndex].patch;
    std::cout << "Rendered '" << patch.name << "' using " << events.size() / 2
              << " scripted notes to " << argv[3] << '\n';
    return 0;
}
