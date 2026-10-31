#include "p6/P6Engine.h"
#include "p6/P6PatchState.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace {

void setRaw(p6::P6PatchState& patch, std::uint16_t id, std::uint16_t raw)
{
    for (auto& parameter : patch.rawParameters) {
        if (parameter.id == id) {
            parameter.raw = raw;
            parameter.recognized = true;
            return;
        }
    }
    patch.rawParameters.push_back({id, raw, 0.0f, true});
}

p6::P6PatchState basePatch()
{
    p6::P6PatchState patch;
    patch.rawParameters.reserve(64);
    setRaw(patch, 0, 24);
    setRaw(patch, 2, 115);
    setRaw(patch, 3, 150);
    setRaw(patch, 4, 127);
    setRaw(patch, 8, 31);
    setRaw(patch, 10, 90);
    setRaw(patch, 11, 180);
    setRaw(patch, 12, 105);
    setRaw(patch, 15, 90);
    setRaw(patch, 16, 82);
    setRaw(patch, 17, 22);
    setRaw(patch, 45, 105);
    setRaw(patch, 46, 120);
    setRaw(patch, 49, 30);
    setRaw(patch, 50, 80);
    setRaw(patch, 62, 100);
    setRaw(patch, 66, 127);
    setRaw(patch, 67, 4);
    setRaw(patch, 68, 45);
    setRaw(patch, 69, 100);
    setRaw(patch, 70, 50);
    setRaw(patch, 135, 0);
    return patch;
}

p6::P6PatchState syncPolyPatch()
{
    auto patch = basePatch();
    setRaw(patch, 5, 1);
    setRaw(patch, 78, 180);
    setRaw(patch, 79, 110);
    setRaw(patch, 80, 1);
    setRaw(patch, 81, 1);
    setRaw(patch, 82, 1);
    setRaw(patch, 83, 1);
    setRaw(patch, 84, 1);
    return patch;
}

p6::P6PatchState fxPatch()
{
    auto patch = basePatch();
    setRaw(patch, 119, 2);
    setRaw(patch, 120, 86);
    setRaw(patch, 121, 115);
    setRaw(patch, 122, 55);
    setRaw(patch, 127, 8);
    setRaw(patch, 128, 52);
    setRaw(patch, 129, 160);
    setRaw(patch, 130, 65);
    setRaw(patch, 135, 1);
    setRaw(patch, 167, 120);
    return patch;
}

std::uint64_t fnvAppend(std::uint64_t hash, std::int32_t value)
{
    constexpr std::uint64_t prime = 1099511628211ull;
    const auto u = static_cast<std::uint32_t>(value);
    for (int shift = 0; shift < 32; shift += 8) {
        hash ^= static_cast<std::uint8_t>((u >> shift) & 0xffu);
        hash *= prime;
    }
    return hash;
}

std::int32_t quantize(double value)
{
    const double bounded = std::clamp(value, -4.0, 4.0);
    return static_cast<std::int32_t>(std::llround(bounded * 8388608.0));
}

struct Fingerprint {
    std::uint64_t hash{1469598103934665603ull};
    double sumSquares{};
    double peak{};
    double stereoDifferenceSquares{};
    double weightedAbs{};
    std::uint64_t sampleCount{};

    void add(double left, double right, std::uint64_t index)
    {
        hash = fnvAppend(hash, quantize(left));
        hash = fnvAppend(hash, quantize(right));
        sumSquares += left * left + right * right;
        peak = std::max(peak, std::max(std::abs(left), std::abs(right)));
        const double d = left - right;
        stereoDifferenceSquares += d * d;
        weightedAbs += (std::abs(left) + std::abs(right)) * static_cast<double>((index % 257u) + 1u);
        sampleCount += 2;
    }
};

Fingerprint render(const p6::P6PatchState& patch, int oversample)
{
    constexpr double sampleRate = 48000.0;
    p6::P6Engine engine;
    engine.prepare(sampleRate, oversample);
    engine.loadPatch(patch);
    engine.setEffectsRuntimeBpm(120.0);

    Fingerprint fp;
    constexpr std::array<int, 3> notes{48, 55, 64};
    for (const int note : notes)
        engine.noteOn(note, 0.82);

    constexpr int totalSamples = 24000;
    for (int i = 0; i < totalSamples; ++i) {
        if (i == 9000)
            engine.noteOff(notes[0]);
        if (i == 12000)
            engine.noteOn(72, 0.67);
        if (i == 16000) {
            engine.noteOff(notes[1]);
            engine.noteOff(notes[2]);
            engine.noteOff(72);
        }
        const auto out = engine.process();
        fp.add(out.left, out.right, static_cast<std::uint64_t>(i));
    }
    return fp;
}

void print(std::string_view name, const Fingerprint& fp)
{
    const double rms = fp.sampleCount > 0
        ? std::sqrt(fp.sumSquares / static_cast<double>(fp.sampleCount)) : 0.0;
    const double stereoRms = fp.sampleCount > 0
        ? std::sqrt(2.0 * fp.stereoDifferenceSquares / static_cast<double>(fp.sampleCount)) : 0.0;

    std::cout << name << ',' << std::hex << fp.hash << std::dec << ','
              << std::fixed << std::setprecision(12)
              << rms << ',' << fp.peak << ',' << stereoRms << ',' << fp.weightedAbs << '\n';
}

} // namespace

int main()
{
    std::cout << "scenario,quantized_fnv1a,rms,peak,stereo_diff_rms,weighted_abs\n";
    print("dry_2x", render(basePatch(), 2));
    print("sync_poly_2x", render(syncPolyPatch(), 2));
    print("fx_2x", render(fxPatch(), 2));
    print("dry_1x", render(basePatch(), 1));
    return 0;
}
