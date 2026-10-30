#include "p6/P6Engine.h"
#include "p6/P6PatchState.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace {

using Clock = std::chrono::steady_clock;

void setRaw(p6::P6PatchState& patch, std::uint16_t id, std::uint16_t raw)
{
    patch.rawParameters.push_back({id, raw, 0.0f, true});
}

p6::P6PatchState benchmarkPatch(bool effectsOn)
{
    p6::P6PatchState patch;
    patch.rawParameters.reserve(64);

    setRaw(patch, 0, 24);
    setRaw(patch, 2, 110);
    setRaw(patch, 3, 155);
    setRaw(patch, 4, 127);
    setRaw(patch, 5, 1);
    setRaw(patch, 8, 28);
    setRaw(patch, 10, 92);
    setRaw(patch, 11, 188);
    setRaw(patch, 12, 110);
    setRaw(patch, 13, 1);
    setRaw(patch, 14, 55);
    setRaw(patch, 15, 100);
    setRaw(patch, 16, 90);
    setRaw(patch, 17, 25);
    setRaw(patch, 45, 118);
    setRaw(patch, 46, 155);
    setRaw(patch, 49, 42);
    setRaw(patch, 50, 90);
    setRaw(patch, 62, 110);
    setRaw(patch, 66, 127);
    setRaw(patch, 67, 0);
    setRaw(patch, 68, 0);
    setRaw(patch, 69, 127);
    setRaw(patch, 70, 20);

    // A moderately expensive serial chain: resonant phaser -> Hall.
    setRaw(patch, 119, 4);
    setRaw(patch, 120, 96);
    setRaw(patch, 121, 170);
    setRaw(patch, 122, 96);
    setRaw(patch, 127, 6);
    setRaw(patch, 128, 70);
    setRaw(patch, 129, 180);
    setRaw(patch, 130, 70);
    setRaw(patch, 135, effectsOn ? 1 : 0);
    setRaw(patch, 167, 120);
    return patch;
}

struct Scenario {
    std::string_view name;
    double sampleRate;
    int oversample;
    int voices;
    bool effects;
    int blockSize;
};

struct Result {
    double cpuPercentOneCore{};
    double averageBlockUs{};
    double worstBlockUs{};
    double realtimeBudgetUs{};
    double checksum{};
};

Result runScenario(const Scenario& scenario)
{
    p6::P6Engine engine;
    engine.prepare(scenario.sampleRate, scenario.oversample);
    engine.loadPatch(benchmarkPatch(scenario.effects));
    engine.setEffectsRuntimeBpm(120.0);

    constexpr std::array<int, 6> notes{48, 55, 60, 64, 67, 72};
    for (int i = 0; i < scenario.voices; ++i)
        engine.noteOn(notes[static_cast<std::size_t>(i)], 0.9);

    const int warmSamples = static_cast<int>(scenario.sampleRate * 0.15);
    double checksum = 0.0;
    for (int i = 0; i < warmSamples; ++i) {
        const auto out = engine.process();
        checksum += out.left * 0.5 + out.right * 0.25;
    }

    constexpr int blocks = 500;
    double worstUs = 0.0;
    double totalBlockUs = 0.0;
    const auto totalStart = Clock::now();
    for (int block = 0; block < blocks; ++block) {
        const auto blockStart = Clock::now();
        for (int sample = 0; sample < scenario.blockSize; ++sample) {
            const auto out = engine.process();
            checksum += out.left * 0.5 + out.right * 0.25;
        }
        const auto blockEnd = Clock::now();
        const double us = std::chrono::duration<double, std::micro>(blockEnd - blockStart).count();
        worstUs = std::max(worstUs, us);
        totalBlockUs += us;
    }
    const auto totalEnd = Clock::now();

    const double processedSamples = static_cast<double>(blocks * scenario.blockSize);
    const double audioSeconds = processedSamples / scenario.sampleRate;
    const double processingSeconds = std::chrono::duration<double>(totalEnd - totalStart).count();
    const double budgetUs = 1.0e6 * static_cast<double>(scenario.blockSize) / scenario.sampleRate;

    return {
        100.0 * processingSeconds / audioSeconds,
        totalBlockUs / static_cast<double>(blocks),
        worstUs,
        budgetUs,
        checksum,
    };
}

} // namespace

int main()
{
    constexpr std::array<Scenario, 8> scenarios{{
        {"48k_1x_1voice_dry", 48000.0, 1, 1, false, 64},
        {"48k_2x_1voice_dry", 48000.0, 2, 1, false, 64},
        {"48k_2x_6voice_dry", 48000.0, 2, 6, false, 64},
        {"48k_2x_6voice_fx", 48000.0, 2, 6, true, 64},
        {"48k_4x_6voice_fx", 48000.0, 4, 6, true, 64},
        {"96k_1x_6voice_fx", 96000.0, 1, 6, true, 64},
        {"96k_2x_6voice_dry", 96000.0, 2, 6, false, 64},
        {"96k_2x_6voice_fx", 96000.0, 2, 6, true, 64},
    }};

    std::cout << "scenario,cpu_percent_one_core,avg_block_us,worst_block_us,budget_us,worst_budget_percent,checksum\n";
    std::cout << std::fixed << std::setprecision(3);
    for (const auto& scenario : scenarios) {
        const auto result = runScenario(scenario);
        std::cout << scenario.name << ','
                  << result.cpuPercentOneCore << ','
                  << result.averageBlockUs << ','
                  << result.worstBlockUs << ','
                  << result.realtimeBudgetUs << ','
                  << (100.0 * result.worstBlockUs / result.realtimeBudgetUs) << ','
                  << std::setprecision(9) << result.checksum << std::setprecision(3)
                  << '\n';
    }
    return 0;
}
