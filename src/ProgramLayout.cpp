#include "p6/ProgramLayout.h"

#include <array>
#include <algorithm>

namespace p6 {
namespace {

// Layout cross-checked against the Prophet-6 MIDI table, factory-bank statistics,
// and independent editor/librarian implementations.  The serialized byte order is
// not always the same as the printed NRPN order (notably two aftertouch destination
// bytes); raw SysEx bytes are always preserved even when metadata is uncertain.
constexpr std::array<SerializedParameterInfo, 87> kLayout{{
    {"Osc1Freq",0,0,0,60,SerializedValueType::Enum},
    {"Osc1Sync",11,1,0,1,SerializedValueType::Boolean},
    {"Osc1Level",7,2,0,127,SerializedValueType::Unsigned},
    {"Osc1Shape",3,3,0,254,SerializedValueType::Unsigned},
    {"Osc1PW",5,4,0,255,SerializedValueType::Unsigned},
    {"Osc2Freq",1,5,0,60,SerializedValueType::Enum},
    {"Osc2FreqFine",2,6,0,254,SerializedValueType::SignedCentered},
    {"Osc2Level",8,7,0,127,SerializedValueType::Unsigned},
    {"Osc2Shape",4,8,0,254,SerializedValueType::Unsigned},
    {"Osc2PW",6,9,0,255,SerializedValueType::Unsigned},
    {"Osc2LoFreq",13,10,0,1,SerializedValueType::Boolean},
    {"Osc2Kbd",12,11,0,1,SerializedValueType::Boolean},
    {"SubOctLevel",9,27,0,127,SerializedValueType::Unsigned},
    {"GlideMode",15,28,0,3,SerializedValueType::Enum},
    {"GlideOn",16,29,0,1,SerializedValueType::Boolean},
    {"GlideRate",14,30,0,127,SerializedValueType::Unsigned},
    {"PitchBendRange",17,31,0,24,SerializedValueType::Enum},
    {"NoiseLevel",10,32,0,127,SerializedValueType::Unsigned},
    {"SlopAmt",18,33,0,127,SerializedValueType::Unsigned},
    {"FiltFreq",19,45,0,164,SerializedValueType::Unsigned},
    {"FiltRes",20,46,0,255,SerializedValueType::Unsigned},
    {"FiltKbdTrack",21,47,0,2,SerializedValueType::Enum},
    {"FiltVel",22,48,0,1,SerializedValueType::Boolean},
    {"FiltFreqHP",23,49,0,164,SerializedValueType::Unsigned},
    {"FiltResHP",24,50,0,255,SerializedValueType::Unsigned},
    {"FiltKbdTrackHP",25,51,0,2,SerializedValueType::Enum},
    {"FiltVelHP",26,52,0,1,SerializedValueType::Boolean},
    {"ProgVolume",27,62,0,127,SerializedValueType::Unsigned},
    {"PanSpread",28,63,0,127,SerializedValueType::Unsigned},
    {"Distortion",58,64,0,127,SerializedValueType::Unsigned},
    {"AmpEnvAmt",31,66,0,127,SerializedValueType::Unsigned},
    {"AmpEnvAttack",36,67,0,127,SerializedValueType::Unsigned},
    {"AmpEnvDecay",38,68,0,127,SerializedValueType::Unsigned},
    {"AmpEnvSustain",40,69,0,127,SerializedValueType::Unsigned},
    {"AmpEnvRelease",42,70,0,127,SerializedValueType::Unsigned},
    {"AmpVel",43,71,0,1,SerializedValueType::Boolean},
    {"FiltEnvAmt",29,77,0,254,SerializedValueType::SignedCentered},
    {"FiltEnvAttack",35,78,0,127,SerializedValueType::Unsigned},
    {"FiltEnvDecay",37,79,0,127,SerializedValueType::Unsigned},
    {"FiltEnvSustain",39,80,0,127,SerializedValueType::Unsigned},
    {"FiltEnvRelease",41,81,0,127,SerializedValueType::Unsigned},
    {"FiltEnvAmtHP",30,82,0,254,SerializedValueType::SignedCentered},
    {"LFOFreq",59,88,0,254,SerializedValueType::Unsigned},
    {"LFOInitAmt",63,89,0,255,SerializedValueType::Unsigned},
    {"LFOShape",62,90,0,4,SerializedValueType::Enum},
    {"LFOSync",61,91,0,1,SerializedValueType::Boolean},
    {"LFODestOsc1",64,93,0,1,SerializedValueType::Boolean},
    {"LFODestOsc2",65,94,0,1,SerializedValueType::Boolean},
    {"LFODestPW",66,95,0,1,SerializedValueType::Enum},
    {"LFODestAmp",69,96,0,1,SerializedValueType::Boolean},
    {"LFODestFiltLP",67,97,0,1,SerializedValueType::Boolean},
    {"LFODestFiltHP",68,98,0,1,SerializedValueType::Boolean},
    {"AftertouchAmt",70,109,0,254,SerializedValueType::SignedCentered},
    {"AftertouchOsc1",71,110,0,1,SerializedValueType::Boolean},
    {"AftertouchOsc2",72,111,0,1,SerializedValueType::Boolean},
    {"AftertouchFiltLP",73,112,0,1,SerializedValueType::Boolean},
    {"AftertouchAmp",74,114,0,1,SerializedValueType::Boolean},
    {"AftertouchFiltHP",75,113,0,1,SerializedValueType::Boolean},
    {"AftertouchLFOAmt",76,115,0,1,SerializedValueType::Boolean},
    {"EffectAType",44,119,0,5,SerializedValueType::Enum},
    {"EffectAMix",48,120,0,127,SerializedValueType::Unsigned},
    {"EffectAParam1",50,121,0,255,SerializedValueType::Unsigned},
    {"EffectAParam2",52,122,0,127,SerializedValueType::Unsigned},
    {"EffectASync",54,123,0,1,SerializedValueType::Boolean},
    {"EffectASyncRate",56,124,0,10,SerializedValueType::Enum},
    {"EffectBType",45,127,0,9,SerializedValueType::Enum},
    {"EffectBMix",49,128,0,127,SerializedValueType::Unsigned},
    {"EffectBParam1",51,129,0,255,SerializedValueType::Unsigned},
    {"EffectBParam2",53,130,0,127,SerializedValueType::Unsigned},
    {"EffectBSync",55,131,0,1,SerializedValueType::Boolean},
    {"EffectBSyncRate",57,132,0,10,SerializedValueType::Enum},
    {"EffectsOn",46,135,0,1,SerializedValueType::Boolean},
    {"PModFiltEnvAmt",77,143,0,254,SerializedValueType::SignedCentered},
    {"PModOsc2Amt",78,144,0,254,SerializedValueType::SignedCentered},
    {"PModOsc1",79,145,0,1,SerializedValueType::Boolean},
    {"PModShape1",80,146,0,1,SerializedValueType::Boolean},
    {"PModPW1",81,147,0,1,SerializedValueType::Boolean},
    {"PModFiltLP",82,148,0,1,SerializedValueType::Boolean},
    {"PModFiltHP",83,149,0,1,SerializedValueType::Boolean},
    {"UnisonOn",84,156,0,1,SerializedValueType::Boolean},
    {"UnisonMode",85,157,0,6,SerializedValueType::Enum},
    {"KeyPriority",86,158,0,5,SerializedValueType::Enum},
    {"ArpOn",91,160,0,1,SerializedValueType::Boolean},
    {"ArpMode",89,161,0,4,SerializedValueType::Enum},
    {"ArpRange",90,162,0,2,SerializedValueType::Enum},
    {"ClockTimeSig",92,163,0,9,SerializedValueType::Enum},
    {"ClockBPM",87,167,30,250,SerializedValueType::Unsigned}
}};

} // namespace

std::span<const SerializedParameterInfo> serializedParameterLayout()
{
    return kLayout;
}

const SerializedParameterInfo* findSerializedByNrpn(std::uint16_t nrpn)
{
    for (const auto& p : kLayout)
        if (p.nrpn == nrpn)
            return &p;
    return nullptr;
}

const SerializedParameterInfo* findSerializedByOffset(std::size_t offset)
{
    for (const auto& p : kLayout)
        if (p.sysexOffset == offset)
            return &p;
    return nullptr;
}

void populateRawParametersFromPayload(P6PatchState& patch)
{
    patch.rawParameters.clear();
    patch.rawParameters.reserve(kLayout.size());
    for (const auto& p : kLayout) {
        const auto rawByte = patch.rawPayload[p.sysexOffset];
        const auto raw = static_cast<std::uint16_t>(rawByte);
        const auto clamped = std::clamp(raw, p.minimum, p.maximum);
        RawParameter rp;
        rp.id = p.nrpn;
        rp.raw = raw; // preserve exact byte, including values outside metadata
        rp.normalized = p.maximum > p.minimum
            ? static_cast<float>(clamped - p.minimum) / static_cast<float>(p.maximum - p.minimum)
            : 0.0f;
        rp.implemented = false;
        patch.rawParameters.push_back(rp);
    }
}

} // namespace p6
