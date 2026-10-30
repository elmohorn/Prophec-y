#include "p6/P6Arpeggiator.h"
#include "p6/P6Clock.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

using namespace p6;

int main(int argc, char** argv)
{
    const std::filesystem::path outDir = argc > 1 ? argv[1] : "p6_timing_probe";
    std::filesystem::create_directories(outDir);

    {
        std::ofstream out(outDir / "clock_divisions_120bpm.csv");
        out << "raw,name,quarter_notes_per_step,samples_48k_120bpm,midi_clocks_per_step,swing_timing_opaque\n";
        P6Clock clock;
        clock.prepare(48000.0);
        clock.setBpm(120.0);
        out << std::fixed << std::setprecision(6);
        for (unsigned raw = 0; raw <= 9; ++raw) {
            const auto d = static_cast<ClockDivision>(raw);
            clock.setDivision(d);
            const auto info = clockDivisionInfo(d);
            out << raw << ',' << clockDivisionName(d) << ','
                << info.quarterNotesPerStep << ',' << clock.nominalStepSamples() << ','
                << clock.midiClocksPerStep() << ',' << (info.swingOpaque ? 1 : 0) << '\n';
        }
    }

    {
        std::ofstream out(outDir / "arp_patterns.csv");
        out << "mode,octaves,step,note\n";
        const struct { ArpMode mode; const char* name; } modes[] = {
            {ArpMode::Up, "up"}, {ArpMode::Down, "down"},
            {ArpMode::UpDown, "up_down"}, {ArpMode::Assign, "assign"}
        };
        for (const auto& m : modes) {
            P6Arpeggiator arp;
            arp.noteOn(67, 0.7);
            arp.noteOn(60, 0.6);
            arp.noteOn(64, 0.8);
            arp.setMode(m.mode);
            arp.setOctaves(2);
            for (int step = 0; step < 12; ++step) {
                const auto n = arp.next();
                if (n) out << m.name << ",2," << step << ',' << n->note << '\n';
            }
        }
    }

    {
        std::ofstream out(outDir / "assign_repeat_pattern.csv");
        out << "step,note\n";
        P6Arpeggiator arp;
        arp.setHold(true);
        arp.setMode(ArpMode::Assign);
        arp.noteOn(60, 1.0); // kept physically down while scratchpad is entered
        arp.noteOn(64, 1.0); arp.noteOff(64);
        arp.noteOn(67, 1.0); arp.noteOff(67);
        arp.noteOn(64, 1.0); arp.noteOff(64);
        for (int step = 0; step < 8; ++step) {
            if (const auto n = arp.next()) out << step << ',' << n->note << '\n';
        }
    }

    {
        std::ofstream out(outDir / "clock_policy.csv");
        out << "item,status,note\n";
        out << "internal_clock,implemented,sample-domain fractional accumulator\n";
        out << "host_ppq_anchor,implemented,phase aligns from host musical position to prevent long-term drift\n";
        out << "midi_clock_24ppqn,implemented,F8 pulse divider with Start Continue Stop state\n";
        out << "8th_swing_ratio,opaque,manual names swing but does not publish long-short ratio; straight average fallback\n";
        out << "16th_swing_ratio,opaque,manual names full swing but does not publish long-short ratio; straight average fallback\n";
        out << "arp_assign_repeat_pitch,implemented,32-press event scratchpad retains repeated pitches; based on P6 field evidence\n";
        out << "arp_updown_endpoint_policy,candidate,no repeated endpoints; cross-checked against mature arp scheduler but not isolated on hardware\n";
        out << "arp_random_rng,opaque,deterministic xorshift for regression only; no claim about Prophet RNG\n";
        out << "sequence_velocity_bit7,opaque,preserved losslessly; no tie-retrigger semantics assigned\n";
        out << "seq_mode_170,opaque,preserved as raw NRPN; playback semantics not guessed\n";
        out << "seq_play_mode_171,opaque,preserved as raw NRPN; playback semantics not guessed\n";
    }

    std::cout << outDir.string() << '\n';
    return 0;
}
