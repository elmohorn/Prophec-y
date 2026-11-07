// Offline Character diagnostic. Amplitude refers to sine input sample peak;
// output and relative level are measured after a 250 ms settling period.
#include "CharacterFx.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>

int main()
{
    constexpr int rate = 48000;
    constexpr double pi = 3.14159265358979323846;
    std::cout << "ignition,mix,input_peak,output_rms,relative_db,conduction\n";
    std::cout << std::setprecision(8);
    for (float ignition : {0.0f, 0.25f, 0.5f, 0.75f})
        for (float mix : {0.25f, 0.5f, 1.0f})
            for (double amplitude : {0.005, 0.01, 0.02, 0.04, 0.05, 0.06, 0.1, 0.25, 0.5}) {
                p6plugin::CharacterFx fx;
                fx.prepare(rate);
                p6plugin::CharacterState state;
                state.type = 2;
                state.custom[0][1] = ignition;
                state.custom[0][5] = mix;
                fx.setState(state);
                double inputPower = 0, outputPower = 0, conduction = 0;
                int samples = 0, telemetrySamples = 0;
                for (int n = 0; n < rate; ++n) {
                    const double value = amplitude * std::sin(2.0 * pi * 440.0 * n / rate);
                    fx.advanceRoute();
                    const auto out = fx.process({value, value});
                    if (!std::isfinite(out.left) || !std::isfinite(out.right)) return 1;
                    if (n >= rate / 4) {
                        inputPower += value * value;
                        outputPower += 0.5 * (out.left * out.left + out.right * out.right);
                        ++samples;
                        if (n % 64 == 0) { conduction += fx.telemetry().device.values[0]; ++telemetrySamples; }
                    }
                }
                const double rms = std::sqrt(outputPower / samples);
                std::cout << ignition << ',' << mix << ',' << amplitude << ',' << rms << ',';
                if (outputPower > 0.0) std::cout << 10.0 * std::log10(outputPower / inputPower);
                std::cout << ',' << conduction / telemetrySamples << '\n';
            }
}
