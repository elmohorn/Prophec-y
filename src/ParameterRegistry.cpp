#include "p6/ParameterRegistry.h"

#include <array>

namespace p6 {
namespace {

constexpr std::array<ParameterInfo, 111> kParameters{{
    {0,0,60,"Osc 1 Freq",ParameterGroup::Oscillator},
    {1,0,1,"Osc 1 Sync",ParameterGroup::Oscillator},
    {2,0,127,"Osc 1 Level",ParameterGroup::Mixer},
    {3,0,254,"Osc 1 Shape",ParameterGroup::Oscillator},
    {4,0,255,"Osc 1 Pulse Width",ParameterGroup::Oscillator},
    {5,0,60,"Osc 2 Freq",ParameterGroup::Oscillator},
    {6,0,254,"Osc 2 Freq Fine",ParameterGroup::Oscillator},
    {7,0,127,"Osc 2 Level",ParameterGroup::Mixer},
    {8,0,254,"Osc 2 Shape",ParameterGroup::Oscillator},
    {9,0,255,"Osc 2 Pulse Width",ParameterGroup::Oscillator},
    {10,0,1,"Osc 2 Low Freq",ParameterGroup::Oscillator},
    {11,0,1,"Osc 2 Key On/Off",ParameterGroup::Oscillator},
    {27,0,127,"Osc 1 Sub Level",ParameterGroup::Mixer},
    {28,0,3,"Glide Mode",ParameterGroup::Glide},
    {29,0,1,"Glide On/Off",ParameterGroup::Glide},
    {30,0,127,"Glide Rate",ParameterGroup::Glide},
    {31,0,24,"Pitch Bend Range",ParameterGroup::Performance},
    {32,0,127,"Noise Level",ParameterGroup::Mixer},
    {33,0,127,"Slop",ParameterGroup::Performance},
    {45,0,164,"Low-pass Freq",ParameterGroup::Filter},
    {46,0,255,"Low-pass Resonance",ParameterGroup::Filter},
    {47,0,2,"Low-pass Key Amount",ParameterGroup::Filter},
    {48,0,1,"Low-pass Velocity",ParameterGroup::Filter},
    {49,0,164,"High-pass Freq",ParameterGroup::Filter},
    {50,0,255,"High-pass Resonance",ParameterGroup::Filter},
    {51,0,2,"High-pass Key Amount",ParameterGroup::Filter},
    {52,0,1,"High-pass Velocity",ParameterGroup::Filter},
    {62,0,127,"Voice Volume",ParameterGroup::Amplifier},
    {63,0,127,"Pan Spread",ParameterGroup::Performance},
    {64,0,127,"Distortion Amount",ParameterGroup::Effects},
    {66,0,127,"VCA Env Amount",ParameterGroup::Amplifier},
    {67,0,127,"VCA Env Attack",ParameterGroup::Amplifier},
    {68,0,127,"VCA Env Decay",ParameterGroup::Amplifier},
    {69,0,127,"VCA Env Sustain",ParameterGroup::Amplifier},
    {70,0,127,"VCA Env Release",ParameterGroup::Amplifier},
    {71,0,1,"VCA Env Velocity",ParameterGroup::Amplifier},
    {77,0,254,"Low-pass Env Amount",ParameterGroup::Filter},
    {78,0,127,"Filter Env Attack",ParameterGroup::Filter},
    {79,0,127,"Filter Env Decay",ParameterGroup::Filter},
    {80,0,127,"Filter Env Sustain",ParameterGroup::Filter},
    {81,0,127,"Filter Env Release",ParameterGroup::Filter},
    {82,0,254,"High-pass Env Amount",ParameterGroup::Filter},
    {88,0,254,"LFO Frequency",ParameterGroup::LFO},
    {89,0,255,"LFO Initial Amount",ParameterGroup::LFO},
    {90,0,4,"LFO Shape",ParameterGroup::LFO},
    {91,0,1,"LFO Sync",ParameterGroup::LFO},
    {93,0,1,"LFO Freq 1 Dest",ParameterGroup::LFO},
    {94,0,1,"LFO Freq 2 Dest",ParameterGroup::LFO},
    {95,0,1,"LFO PW 1+2 Dest",ParameterGroup::LFO},
    {96,0,1,"LFO Amp Dest",ParameterGroup::LFO},
    {97,0,1,"LFO Low-pass Dest",ParameterGroup::LFO},
    {98,0,1,"LFO High-pass Dest",ParameterGroup::LFO},
    {109,0,254,"Pressure Amount",ParameterGroup::Pressure},
    {110,0,1,"Pressure Freq 1 Dest",ParameterGroup::Pressure},
    {111,0,1,"Pressure Freq 2 Dest",ParameterGroup::Pressure},
    {112,0,1,"Pressure Low-pass Dest",ParameterGroup::Pressure},
    {113,0,1,"Pressure High-pass Dest",ParameterGroup::Pressure},
    {114,0,1,"Pressure VCA Dest",ParameterGroup::Pressure},
    {115,0,1,"Pressure LFO Amount Dest",ParameterGroup::Pressure},
    {119,0,5,"FX 1 Select",ParameterGroup::Effects},
    {120,0,127,"FX 1 Mix",ParameterGroup::Effects},
    {121,0,255,"FX 1 Param 1",ParameterGroup::Effects},
    {122,0,127,"FX 1 Param 2",ParameterGroup::Effects},
    {123,0,1,"FX 1 Sync",ParameterGroup::Effects},
    {124,0,10,"FX 1 Sync Rate",ParameterGroup::Effects},
    {127,0,9,"FX 2 Select",ParameterGroup::Effects},
    {128,0,127,"FX 2 Mix",ParameterGroup::Effects},
    {129,0,255,"FX 2 Param 1",ParameterGroup::Effects},
    {130,0,127,"FX 2 Param 2",ParameterGroup::Effects},
    {131,0,1,"FX 2 Sync",ParameterGroup::Effects},
    {132,0,10,"FX 2 Sync Rate",ParameterGroup::Effects},
    {135,0,1,"FX On/Off",ParameterGroup::Effects},
    {143,0,254,"PolyMod Filter Env Amount",ParameterGroup::PolyMod},
    {144,0,254,"PolyMod Osc 2 Amount",ParameterGroup::PolyMod},
    {145,0,1,"PolyMod Freq 1 Dest",ParameterGroup::PolyMod},
    {146,0,1,"PolyMod Shape 1 Dest",ParameterGroup::PolyMod},
    {147,0,1,"PolyMod PW 1 Dest",ParameterGroup::PolyMod},
    {148,0,1,"PolyMod Low-pass Dest",ParameterGroup::PolyMod},
    {149,0,1,"PolyMod High-pass Dest",ParameterGroup::PolyMod},
    {156,0,1,"Unison On/Off",ParameterGroup::Performance},
    {157,0,6,"Unison Mode",ParameterGroup::Performance},
    {158,0,5,"Key Mode",ParameterGroup::Performance},
    {160,0,1,"Arp On/Off",ParameterGroup::Arpeggiator},
    {161,0,4,"Arp Mode",ParameterGroup::Arpeggiator},
    {162,0,2,"Arp Range",ParameterGroup::Arpeggiator},
    {163,0,9,"Arp Time Signature",ParameterGroup::Arpeggiator},
    {167,30,250,"BPM",ParameterGroup::Arpeggiator},
    {168,0,1,"Sequencer On/Off",ParameterGroup::Sequencer},
    {169,0,63,"Sequencer Length",ParameterGroup::Sequencer},
    {170,0,1,"Sequencer Mode",ParameterGroup::Sequencer},
    {171,0,1,"Sequencer Play Mode",ParameterGroup::Sequencer},
    {236,32,125,"Name Character 0",ParameterGroup::Name},
    {237,32,125,"Name Character 1",ParameterGroup::Name},
    {238,32,125,"Name Character 2",ParameterGroup::Name},
    {239,32,125,"Name Character 3",ParameterGroup::Name},
    {240,32,125,"Name Character 4",ParameterGroup::Name},
    {241,32,125,"Name Character 5",ParameterGroup::Name},
    {242,32,125,"Name Character 6",ParameterGroup::Name},
    {243,32,125,"Name Character 7",ParameterGroup::Name},
    {244,32,125,"Name Character 8",ParameterGroup::Name},
    {245,32,125,"Name Character 9",ParameterGroup::Name},
    {246,32,125,"Name Character 10",ParameterGroup::Name},
    {247,32,125,"Name Character 11",ParameterGroup::Name},
    {248,32,125,"Name Character 12",ParameterGroup::Name},
    {249,32,125,"Name Character 13",ParameterGroup::Name},
    {250,32,125,"Name Character 14",ParameterGroup::Name},
    {251,32,125,"Name Character 15",ParameterGroup::Name},
    {252,32,125,"Name Character 16",ParameterGroup::Name},
    {253,32,125,"Name Character 17",ParameterGroup::Name},
    {254,32,125,"Name Character 18",ParameterGroup::Name},
    {255,32,125,"Name Character 19",ParameterGroup::Name}
}};

} // namespace

std::span<const ParameterInfo> programParameterRegistry()
{
    return kParameters;
}

const ParameterInfo* findProgramParameter(std::uint16_t nrpn)
{
    for (const auto& p : kParameters)
        if (p.nrpn == nrpn)
            return &p;
    return nullptr;
}

} // namespace p6
