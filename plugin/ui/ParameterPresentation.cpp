#include "ParameterPresentation.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace p6plugin::ui {
namespace {
std::string number(double v,int precision)
{
    std::ostringstream out;out.imbue(std::locale::classic());
    out<<std::fixed<<std::setprecision(precision)<<v;return out.str();
}
std::string_view trim(std::string_view s)
{
    const auto first=s.find_first_not_of(" \t\r\n");
    if(first==std::string_view::npos)return {};
    return s.substr(first,s.find_last_not_of(" \t\r\n")-first+1);
}
}
PresentedValue parameterValue(int nrpn,double raw,const ParameterContext& context)
{
    const auto& c=context.calibration;
    const auto* info=p6::findProgramParameter(static_cast<std::uint16_t>(nrpn));
    if(info)raw=std::clamp(raw,static_cast<double>(info->minimum),static_cast<double>(info->maximum));
    const auto discrete=static_cast<std::uint16_t>(std::clamp(std::round(raw),0.,16383.));
    switch(nrpn) {
    case 0: case 5:return {raw-c.oscCoarseCenterRaw,ValueUnit::Semitones};
    case 6:return {p6::signedCentered01(discrete,127)*c.osc2FineRangeCents,ValueUnit::Cents};
    case 3: case 8:return {raw/254.,ValueUnit::Morph};
    case 4: case 9:return {p6::mappedPulseWidth(discrete,c)*100.,ValueUnit::Percent};
    case 2: case 7: case 27: case 32: case 33: case 62: case 63:
    case 69: case 80: case 120: case 128:return {raw/127.*100.,ValueUnit::Percent};
    case 46: case 89:return {raw/255.*100.,ValueUnit::Percent};
    case 45: case 49:return {raw/164.*100.,ValueUnit::Percent};
    case 77: case 143: case 144:return {p6::signedCentered01(discrete,127)*100.,ValueUnit::Percent};
    case 88:return context.lfoSynced?PresentedValue{raw,ValueUnit::Raw}:PresentedValue{p6::rawLfoHz(discrete),ValueUnit::Hertz};
    case 67: case 68: case 70: case 78: case 79: case 81:
        return {p6::mappedEnvelopeTime(static_cast<std::uint16_t>(nrpn),discrete,c),ValueUnit::Seconds};
    case 30:return {p6::rawGlideTime(discrete),ValueUnit::Seconds};
    default:return {raw,ValueUnit::Raw};
    }
}
std::string formatParameterValue(int nrpn,double raw,const ParameterContext& context)
{
    const auto v=parameterValue(nrpn,raw,context);
    switch(v.unit) {
    case ValueUnit::Seconds:return v.value<1.?number(v.value*1000.,v.value<.1?1:0)+" ms":number(v.value,v.value<10.?2:1)+" s";
    case ValueUnit::Semitones:return std::string(v.value>=0.?"+":"")+number(v.value,0)+" st";
    case ValueUnit::Cents:return std::string(v.value>=0.?"+":"")+number(v.value,1)+" cent";
    case ValueUnit::Hertz:return number(v.value,2)+" Hz";
    case ValueUnit::Percent:return number(v.value,1)+((nrpn==4||nrpn==9)?" % PW":" %");
    case ValueUnit::Morph:return v.value<.5?"TRI > SAW  "+number(v.value*200.,0)+" %":"SAW > PULSE  "+number((v.value-.5)*200.,0)+" %";
    case ValueUnit::Raw:return (nrpn==88&&context.lfoSynced?"SYNC RAW ":"")+number(v.value,0);
    }
    return {};
}
std::optional<double> parseParameterValue(int nrpn,std::string_view text,const ParameterContext& context,bool rawInput)
{
    const auto* info=p6::findProgramParameter(static_cast<std::uint16_t>(nrpn));
    if(!info)return {};
    std::string input(trim(text));
    if(input.empty())return {};
    // One decimal comma is accepted for typed values, but never a thousands
    // separator or an expression. Invalid input cannot silently become zero.
    if(input.find(',')!=std::string::npos) {
        if(input.find('.')!=std::string::npos||std::count(input.begin(),input.end(),',')!=1)return {};
        std::replace(input.begin(),input.end(),',','.');
    }
    std::string_view valueText=input;
    if(valueText.front()=='+')valueText.remove_prefix(1);
    double value{};const auto parsed=std::from_chars(valueText.data(),valueText.data()+valueText.size(),value);
    if(parsed.ec!=std::errc{}||!std::isfinite(value))return {};
    const auto suffix=trim(std::string_view(parsed.ptr,static_cast<std::size_t>(valueText.data()+valueText.size()-parsed.ptr)));
    const auto unit=rawInput?ValueUnit::Raw:parameterValue(nrpn,info->minimum,context).unit;
    switch(unit) {
    case ValueUnit::Raw:
        if(!suffix.empty()&&suffix!="raw")return {};
        if(value<static_cast<double>(info->minimum)||value>info->maximum||std::round(value)!=value)return {};
        return value;
    case ValueUnit::Seconds:
        if(suffix=="ms")value/=1000.;else if(!suffix.empty()&&suffix!="s")return {};break;
    case ValueUnit::Percent:if(!suffix.empty()&&suffix!="%"&&suffix!="% PW")return {};break;
    case ValueUnit::Hertz:if(!suffix.empty()&&suffix!="Hz")return {};break;
    case ValueUnit::Semitones:if(!suffix.empty()&&suffix!="st")return {};break;
    case ValueUnit::Cents:if(!suffix.empty()&&suffix!="cent")return {};break;
    case ValueUnit::Morph:return {}; // Morph labels are not a scalar unit; use raw entry.
    }
    const auto lo=parameterValue(nrpn,info->minimum,context).value;
    const auto hi=parameterValue(nrpn,info->maximum,context).value;
    if(value<std::min(lo,hi)-1.e-9||value>std::max(lo,hi)+1.e-9)return {};
    double best=info->minimum,error=std::numeric_limits<double>::max();
    // Invert the exact forward conversion over this discrete hardware domain,
    // including plateaus and calibration changes, rather than duplicate laws.
    for(unsigned raw=info->minimum;raw<=info->maximum;++raw) {
        const double distance=std::abs(parameterValue(nrpn,raw,context).value-value);
        if(distance<error){error=distance;best=raw;}
    }
    return best;
}
std::string parameterHelp(int nrpn)
{
    if(nrpn==45)return "Low-pass cutoff control. Percent is the hardware control position, not measured Hz. Keyboard tracking, Filter Envelope, LFO and Poly Mod may affect each voice. Shift-drag for finer adjustment; arrows edit one raw step. Double-click resets to Init.";
    return "Values follow the selected software model/calibration. Raw Prophet values remain canonical.";
}
} // namespace p6plugin::ui
