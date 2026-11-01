#include "ui/ParameterPresentation.h"
#include "p6/NrpnCodec.h"
#include "p6/ProgramLayout.h"
#include <cmath>
#include <iostream>
#include <limits>
#define CHECK(x) do{if(!(x)){std::cerr<<"failed: " #x <<" line "<<__LINE__<<'\n';return 1;}}while(false)
int main()
{
    using namespace p6plugin::ui;
    for(const auto& calibration:{p6::MapperCalibration{},p6::referenceACandidateV09()}) {
        ParameterContext context{calibration,false};
        for(int nrpn:{67,68,70,78,79,81})for(int raw=0;raw<=127;++raw) {
            p6::P6PatchState patch;p6::populateRawParametersFromPayload(patch);
            p6::applyRawNrpn(patch,{static_cast<std::uint16_t>(nrpn),static_cast<std::uint16_t>(raw)});
            const auto mapped=p6::mapPatchToDSP(patch,calibration);
            const double expected=nrpn==67?mapped.voice.ampAttack:nrpn==68?mapped.voice.ampDecay:
                nrpn==70?mapped.voice.ampRelease:nrpn==78?mapped.voice.filterAttack:
                nrpn==79?mapped.voice.filterDecay:mapped.voice.filterRelease;
            CHECK(parameterValue(nrpn,raw,context).value==expected);
            const auto parsed=parseParameterValue(nrpn,formatParameterValue(nrpn,raw,context),context);
            CHECK(parsed.has_value());
            // Printed time is rounded, so compare its represented physical
            // value; floor plateaus legitimately have several raw encodings.
            CHECK(std::abs(parameterValue(nrpn,*parsed,context).value-expected)<.012);
        }
    }
    ParameterContext base,reference{p6::referenceACandidateV09(),false};
    CHECK(formatParameterValue(78,64,base)=="908 ms");
    CHECK(formatParameterValue(78,64,reference)=="320 ms");
    CHECK(parseParameterValue(45,"50 %",base)==82.);
    CHECK(parseParameterValue(45,"50,0 %",base)==82.);
    CHECK(parseParameterValue(45,"164",base,true)==164.);
    for(const auto* invalid:{"", "nan", "inf", "garbage", "101 %", "-1 %", "50 Hz", "50 % garbage", "1,2,3", "1.5"})
        CHECK(!parseParameterValue(45,invalid,base,true).has_value());
    CHECK(!parseParameterValue(45,"101 %",base));
    CHECK(!parseParameterValue(45,"nan",base));
    CHECK(!parseParameterValue(45,"50 Hz",base));
    CHECK(!parseParameterValue(45,"165",base,true));
    for(int raw=0;raw<=164;++raw)
        CHECK(parseParameterValue(45,formatParameterValue(45,raw,base),base)==static_cast<double>(raw));
    reference.lfoSynced=true;
    CHECK(formatParameterValue(88,100,reference)=="SYNC RAW 100");
    CHECK(!parseParameterValue(88,"3 Hz",reference));
    CHECK(formatParameterValue(88,100,base).find("Hz")!=std::string::npos);
    CHECK(formatParameterValue(49,164,base).find("OPEN")==std::string::npos);
    std::cout<<"Parameter presentation matches effective mapper profiles, discrete parsing and synced-rate policy\n";
}
