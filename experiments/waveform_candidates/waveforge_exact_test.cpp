#include <array>
#include <cmath>
#include <iostream>
namespace juce {template<class T> T jmin(T a,T b){return a<b?a:b;}}
constexpr int WAVETABLE_SIZE=2048, NUM_WAVETABLES=8;
struct WavetableOscillator {const std::array<std::array<float,WAVETABLE_SIZE>,NUM_WAVETABLES>* wavetableBank=nullptr;float phase=0,phaseIncrement=0,tablePosition=0;float getNextSample();};
float WavetableOscillator::getNextSample()
{
    if (wavetableBank == nullptr)
        return 0.0f;

    // Get interpolated position in table
    float tableIndex = phase * WAVETABLE_SIZE;
    int index0 = static_cast<int>(tableIndex);
    int index1 = (index0 + 1) % WAVETABLE_SIZE;
    float frac = tableIndex - index0;

    // Interpolate between wavetables
    int table0 = static_cast<int>(tablePosition);
    int table1 = juce::jmin(table0 + 1, NUM_WAVETABLES - 1);
    float tableFrac = tablePosition - table0;

    // Get samples from both tables
    const auto& bank = *wavetableBank;
    float sampleT0 = bank[table0][index0] + frac * (bank[table0][index1] - bank[table0][index0]);
    float sampleT1 = bank[table1][index0] + frac * (bank[table1][index1] - bank[table1][index0]);

    float sample = sampleT0 + tableFrac * (sampleT1 - sampleT0);

    // Advance phase
    phase += phaseIncrement;
    if (phase >= 1.0f) phase -= 1.0f;

    return sample;
}
int main(){std::array<std::array<float,WAVETABLE_SIZE>,NUM_WAVETABLES> bank{};for(int i=0;i<WAVETABLE_SIZE;++i){bank[0][i]=std::sin(6.283185307179586*i/WAVETABLE_SIZE);bank[1][i]=-bank[0][i];}WavetableOscillator osc;osc.wavetableBank=&bank;osc.tablePosition=.5f;double err=0,signal=0;for(int i=0;i<WAVETABLE_SIZE;++i){osc.phase=float(i)/WAVETABLE_SIZE;float audio=osc.getNextSample();float displayed=bank[int(osc.tablePosition)][i];err+=(audio-displayed)*(audio-displayed);signal+=displayed*displayed;}std::cout<<"actual upstream getNextSample vs UI table select: rms_display="<<std::sqrt(signal/WAVETABLE_SIZE)<<" rms_difference="<<std::sqrt(err/WAVETABLE_SIZE)<<"\n";}
