#include "p6/PolyBLEPOscillator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
int checkedOversampling(int v)
{
    if (v != 1 && v != 2 && v != 4 && v != 8)
        throw std::runtime_error("oversampling must be 1, 2, 4, or 8");
    return v;
}

double midiHz(double note)
{
    return 440.0 * std::pow(2.0, (note - 69.0) / 12.0);
}

p6::HardSyncCorrectionMode parseMode(const std::string& s)
{
    if (s == "legacy") return p6::HardSyncCorrectionMode::LegacyStepOnly;
    if (s == "step-slope") return p6::HardSyncCorrectionMode::StepAndSlope;
    throw std::runtime_error("--correction must be legacy or step-slope");
}
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr
            << "usage: p6_sync_microscope <trace.csv> [--note 41] [--slave-semis 28] [--master-semis 0] "
               "[--shape1 0..1] [--shape2 0..1] [--pw1 0.04..0.96] [--pw2 0.04..0.96] "
               "[--oversampling 1|2|4|8] [--cycles N] [--reset-phase 0..1] [--trigger-phase 0..1] "
               "[--slave-phase 0..1] [--master-phase 0..1] [--correction legacy|step-slope] [--events-only]\n";
        return 2;
    }

    int note = 41;
    double slaveSemis = 28.0;
    double masterSemis = 0.0;
    double shape1 = 0.5, shape2 = 0.5;
    double pw1 = 0.5, pw2 = 0.5;
    int oversampling = 8;
    int cycles = 12;
    double resetPhase = 0.0, triggerPhase = 0.0;
    double slavePhase = 0.411, masterPhase = 0.173;
    bool eventsOnly = false;
    auto correctionMode = p6::HardSyncCorrectionMode::LegacyStepOnly;

    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* option) -> std::string {
            if (i + 1 >= argc) throw std::runtime_error(std::string("missing value for ") + option);
            return argv[++i];
        };
        if (a == "--note") note = std::clamp(std::stoi(next("--note")), 0, 127);
        else if (a == "--slave-semis") slaveSemis = std::stod(next("--slave-semis"));
        else if (a == "--master-semis") masterSemis = std::stod(next("--master-semis"));
        else if (a == "--shape1") shape1 = std::clamp(std::stod(next("--shape1")), 0.0, 1.0);
        else if (a == "--shape2") shape2 = std::clamp(std::stod(next("--shape2")), 0.0, 1.0);
        else if (a == "--pw1") pw1 = std::clamp(std::stod(next("--pw1")), 0.04, 0.96);
        else if (a == "--pw2") pw2 = std::clamp(std::stod(next("--pw2")), 0.04, 0.96);
        else if (a == "--oversampling") oversampling = checkedOversampling(std::stoi(next("--oversampling")));
        else if (a == "--cycles") cycles = std::clamp(std::stoi(next("--cycles")), 1, 10000);
        else if (a == "--reset-phase") resetPhase = std::stod(next("--reset-phase"));
        else if (a == "--trigger-phase") triggerPhase = std::stod(next("--trigger-phase"));
        else if (a == "--slave-phase") slavePhase = std::stod(next("--slave-phase"));
        else if (a == "--master-phase") masterPhase = std::stod(next("--master-phase"));
        else if (a == "--correction") correctionMode = parseMode(next("--correction"));
        else if (a == "--events-only") eventsOnly = true;
        else throw std::runtime_error("unknown option: " + a);
    }

    constexpr double hostRate = 48000.0;
    const double internalRate = hostRate * static_cast<double>(oversampling);
    const double masterHz = midiHz(static_cast<double>(note) + masterSemis);
    const double slaveHz = midiHz(static_cast<double>(note) + slaveSemis);

    p6::PolyBLEPOscillator master, slave;
    master.prepare(internalRate);
    slave.prepare(internalRate);
    master.reset(masterPhase);
    slave.reset(slavePhase);
    master.setFrequency(masterHz);
    slave.setFrequency(slaveHz);
    master.setShape(shape2);
    slave.setShape(shape1);
    master.setPulseWidth(pw2);
    slave.setPulseWidth(pw1);
    slave.setSyncResetPhase(resetPhase);
    slave.setHardSyncCorrectionMode(correctionMode);

    std::ofstream out(argv[1]);
    if (!out) {
        std::cerr << "cannot open output CSV\n";
        return 3;
    }
    out << std::setprecision(17);
    out << "internal_sample,time_s,event_index,sync,master_phase_before,slave_phase_before,sync_fraction,"
           "master_out,slave_out,phase_before_reset,reset_phase,value_before_reset,value_after_reset,value_jump,"
           "slope_before_per_sample,slope_after_per_sample,slope_jump_per_sample,step_corr_before,step_corr_after,"
           "slope_corr_before,slope_corr_after,total_corr_before,total_corr_after,end_phase\n";

    std::uint64_t sample = 0;
    int eventIndex = 0;
    const std::uint64_t safetyLimit = static_cast<std::uint64_t>(std::ceil((cycles + 4.0) * internalRate / std::max(masterHz, 0.01)));
    while (eventIndex < cycles && sample < safetyLimit) {
        const double masterBefore = master.phase();
        const double slaveBefore = slave.phase();
        const auto syncAt = master.nextPhaseCrossingFraction(triggerPhase);
        const double masterOut = master.process();
        const double slaveOut = slave.process(syncAt);
        const auto& tr = slave.lastHardSyncTrace();
        if (tr.occurred)
            ++eventIndex;

        if (!eventsOnly || tr.occurred) {
            out << sample << ',' << (static_cast<double>(sample) / internalRate) << ','
                << eventIndex << ',' << (tr.occurred ? 1 : 0) << ','
                << masterBefore << ',' << slaveBefore << ',';
            if (tr.occurred) {
                out << tr.syncFraction << ',' << masterOut << ',' << slaveOut << ','
                    << tr.phaseBeforeReset << ',' << tr.resetPhase << ','
                    << tr.valueBeforeReset << ',' << tr.valueAfterReset << ',' << tr.valueJump << ','
                    << tr.slopeBeforePerSample << ',' << tr.slopeAfterPerSample << ',' << tr.slopeJumpPerSample << ','
                    << tr.stepCorrectionBefore << ',' << tr.stepCorrectionAfter << ','
                    << tr.slopeCorrectionBefore << ',' << tr.slopeCorrectionAfter << ','
                    << tr.totalCorrectionBefore << ',' << tr.totalCorrectionAfter << ',' << tr.endPhase;
            } else {
                out << "," << masterOut << ',' << slaveOut << ",,,,,,,,,,,,,,,";
            }
            out << '\n';
        }
        ++sample;
    }

    if (eventIndex < cycles) {
        std::cerr << "safety limit reached before requested cycle count\n";
        return 4;
    }

    std::cout << "hard-sync microscope: masterHz=" << masterHz
              << " slaveHz=" << slaveHz
              << " ratio=" << slaveHz / masterHz
              << " oversampling=" << oversampling
              << " cycles=" << cycles
              << " correction=" << (correctionMode == p6::HardSyncCorrectionMode::StepAndSlope ? "step-slope" : "legacy")
              << " rows=" << (eventsOnly ? cycles : sample) << '\n';
    return 0;
}
