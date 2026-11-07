// Offline boundedness probe using the production P6Voice signal path.
#include "p6/P6Voice.h"
#include <algorithm>
#include <cmath>
#include <iostream>

int main()
{
    double peak = 0.0, prePeak = 0.0, modulationPeak = 0.0;
    int cases = 0;
    for (double sampleRate : {44100.0, 48000.0, 96000.0})
        for (int oversampling : {1, 2, 4})
            for (int note : {36, 60, 96, 119})
                for (double semitones : {-12.0, 0.0, 12.0})
                    for (int mode = 0; mode < 5; ++mode)
                        for (unsigned seed : {1u, 7u, 31u}) {
                            p6::P6Voice voice;
                            voice.prepare(sampleRate, seed, oversampling);
                            p6::VoiceParams params;
                            params.sync = true;
                            params.osc1.level = 0.5;
                            params.osc2.level = 0.4;
                            params.osc2.semitones = semitones;
                            params.osc1.shape = 0.5;
                            params.osc1.pulseWidth = 0.5;
                            params.ampAttack = 0.005;
                            params.filterAttack = 0.005;
                            params.filterDecay = 0.08;
                            params.filterSustain = 0.3;
                            params.filterRelease = 0.04;
                            params.polyMod.osc2Amount = mode ? 1.0 : 0.0;
                            params.polyMod.filterEnvAmount = mode == 4 ? 0.75 : 0.0;
                            params.polyMod.osc1FreqAmount = mode == 1 || mode == 4 ? 4.0 : 0.0;
                            params.polyMod.osc1ShapeAmount = mode == 2 || mode == 4 ? 0.5 : 0.0;
                            params.polyMod.osc1PulseWidthAmount = mode == 3 || mode == 4 ? 0.48 : 0.0;
                            voice.setParams(params);
                            voice.noteOn(note, 0.8);
                            for (int i = 0; i < static_cast<int>(sampleRate * 0.10); ++i) {
                                if (i == static_cast<int>(sampleRate * 0.07)) voice.noteOff();
                                const auto sample = voice.process();
                                if (!std::isfinite(sample.dry) || !std::isfinite(sample.preFilter) ||
                                    !std::isfinite(sample.polyModSource) || std::abs(sample.dry) > 20.0 ||
                                    std::abs(sample.preFilter) > 100.0) {
                                    std::cerr << "unbounded voice: rate=" << sampleRate << " oversampling=" << oversampling
                                              << " note=" << note << " semitones=" << semitones << " mode=" << mode
                                              << " seed=" << seed << " sample=" << i << " dry=" << sample.dry
                                              << " pre=" << sample.preFilter << '\n';
                                    return 1;
                                }
                                peak = std::max(peak, std::abs(sample.dry));
                                prePeak = std::max(prePeak, std::abs(sample.preFilter));
                                modulationPeak = std::max(modulationPeak, std::abs(sample.polyModSource));
                            }
                            ++cases;
                        }
    std::cout << "cases=" << cases << " dry_peak=" << peak << " pre_peak=" << prePeak
              << " polymod_source_peak=" << modulationPeak << '\n';
}
