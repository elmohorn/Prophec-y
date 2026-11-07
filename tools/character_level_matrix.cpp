// Offline diagnostic: compare default Character devices on identical inputs.
// RMS ratios are electrical levels, not perceptual loudness or hardware parity.
#include "CharacterFx.h"
#include "p6/P6Engine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

int main()
{
    constexpr int sr = 48000, frames = sr * 3;
    constexpr double pi = 3.14159265358979323846;
    std::cout << "input,scale,device,input_rms,output_rms,relative_db,output_peak\n";
    std::cout << std::setprecision(10);
    for (int source = 0; source < 3; ++source) {
        std::vector<p6::StereoSample> input(frames);
        p6::P6Engine engine; engine.prepare(sr, 2);
        for (int n = 0; n < frames; ++n) {
            if (source == 0) {
                const double s = std::sin(2.0 * pi * 440.0 * n / sr);
                input[n] = {s, s};
            } else {
                if (n == 0) {
                    engine.noteOn(48, .8);
                    if (source == 2) { engine.noteOn(55, .8); engine.noteOn(60, .8); engine.noteOn(64, .8); }
                }
                if (n == sr * 2) {
                    engine.noteOff(48);
                    if (source == 2) { engine.noteOff(55); engine.noteOff(60); engine.noteOff(64); }
                }
                input[n] = engine.process();
            }
        }
        for (double scale : {.03, .1, .25, .5}) {
            for (int type = 0; type < p6plugin::kCharacterTypes; ++type) {
                p6plugin::CharacterFx fx; fx.prepare(sr);
                p6plugin::CharacterState state; state.type = type; fx.setState(state);
                double inPower = 0, outPower = 0, peak = 0;
                for (int n = 0; n < frames; ++n) {
                    const p6::StereoSample x { input[n].left * scale, input[n].right * scale };
                    fx.advanceRoute(); const auto y = fx.process(x);
                    if (!std::isfinite(y.left) || !std::isfinite(y.right)) return 1;
                    inPower += .5 * (x.left*x.left + x.right*x.right);
                    outPower += .5 * (y.left*y.left + y.right*y.right);
                    peak = std::max({peak,std::abs(y.left),std::abs(y.right)});
                }
                const double inRms = std::sqrt(inPower/frames), outRms = std::sqrt(outPower/frames);
                std::cout << (source == 0 ? "sine" : source == 1 ? "single_note" : "four_note_chord")
                          << ',' << scale << ',' << p6plugin::kCharacterNames[type] << ','
                          << inRms << ',' << outRms << ',' << 20 * std::log10(outRms/inRms) << ',' << peak << '\n';
            }
        }
    }
}
