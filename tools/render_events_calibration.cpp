#include "p6/P6Engine.h"
#include "p6/SysexStream.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct NoteEvent { double time{}; int note{}; double velocity{}; bool on{}; };
void w16(std::ofstream& f, std::uint16_t v){ const char b[2]={char(v&255),char((v>>8)&255)}; f.write(b,2); }
void w32(std::ofstream& f, std::uint32_t v){ const char b[4]={char(v&255),char((v>>8)&255),char((v>>16)&255),char((v>>24)&255)}; f.write(b,4); }
bool writeFloatWav(const std::string& path,const std::vector<float>& x,int sr){
    std::ofstream f(path,std::ios::binary); if(!f)return false;
    const auto bytes=std::uint32_t(x.size()*sizeof(float));
    f.write("RIFF",4);w32(f,36+bytes);f.write("WAVE",4);f.write("fmt ",4);w32(f,16);w16(f,3);w16(f,2);
    w32(f,sr);w32(f,sr*8);w16(f,8);w16(f,32);f.write("data",4);w32(f,bytes);
    f.write(reinterpret_cast<const char*>(x.data()),std::streamsize(bytes)); return bool(f);
}
std::vector<std::uint8_t> readBytes(const std::string& path){ std::ifstream in(path,std::ios::binary); if(!in)throw std::runtime_error("cannot open SysEx"); return {std::istreambuf_iterator<char>(in),{}}; }
std::vector<NoteEvent> readEvents(const std::string& path){
    std::ifstream in(path); if(!in)throw std::runtime_error("cannot open event CSV");
    std::vector<NoteEvent> ev; std::string line; std::size_t lineNo=0;
    while(std::getline(in,line)){ ++lineNo; if(line.empty()||line[0]=='#')continue; if(lineNo==1&&line.find("time")!=std::string::npos)continue;
        std::replace(line.begin(),line.end(),',',' '); std::istringstream ss(line); double t=0,v=1,d=0; int n=60;
        if(!(ss>>t>>n>>v>>d))throw std::runtime_error("bad event CSV line "+std::to_string(lineNo));
        n=std::clamp(n,0,127);v=std::clamp(v,0.0,1.0);ev.push_back({t,n,v,true});ev.push_back({t+d,n,0,false}); }
    std::stable_sort(ev.begin(),ev.end(),[](const auto&a,const auto&b){return a.time!=b.time?a.time<b.time:(!a.on&&b.on);}); return ev;
}
}
int main(int argc,char**argv){
    if(argc<4){std::cerr<<"usage: p6_render_events_cal <program.syx> <events.csv> <out.wav> [--pw-inset N] [--pw-exponent N] [--pulse-normalization N] [--mixer-exponent N] [--sub-gain N] [--noise-gain N] [--filter-scale N] [--filter-base-hz N] [--filter-slope N] [--hp-filter-base-hz N] [--hp-filter-slope N] [--hp-filter-curve-exp N] [--hp-filter-top-hz N] [--filter-curve-exp N] [--filter-top-hz N] [--filter-knee-raw N] [--filter-high-slope N] [--lp-res-scale N] [--filter-env-octaves N] [--env-exponent N] [--amp-env-exponent N] [--filter-env-exponent N] [--env-max-seconds N] [--amp-env-max-seconds N] [--filter-env-max-seconds N] [--tail-seconds N] [--pre-roll-seconds N] [--pm-pitch-octaves N] [--pm-shape-range N] [--pm-pw-range N] [--pm-filter-octaves N] [--sync-reset-phase N] [--sync-trigger-phase N] [--sync-slope-correction] [--osc2-mixer-polarity +/-1] [--oversampling 1|2|4|8] [--osc-only|--osc1-only|--osc2-only|--sync-only] [--filter-bypass] [--poly-mod-bypass] [--reference-a-v07] [--reference-a-v08] [--reference-a-v09] [--vintage]\n";return 2;}
    p6::MapperCalibration c; double filterScale=1.0,tail=2.0,preRoll=0.0; int oversampling=2; p6::VoiceDiagnostics diagnostics; bool isolationDiagnosticsActive=false;
    for(int i=4;i<argc;++i){std::string a=argv[i];if(a=="--reference-a-v07")c=p6::referenceACandidateV07();else if(a=="--reference-a-v08")c=p6::referenceACandidateV08();else if(a=="--reference-a-v09")c=p6::referenceACandidateV09();}
    for(int i=4;i<argc;++i){std::string a=argv[i];auto next=[&](const char*o){if(i+1>=argc)throw std::runtime_error(std::string("missing value for ")+o);return std::stod(argv[++i]);};
        if(a=="--pw-inset")c.pulseWidthEndInset=std::clamp(next("--pw-inset"),0.0,0.45);
        else if(a=="--pw-exponent")c.pulseWidthCurveExponent=std::clamp(next("--pw-exponent"),0.1,4.0);else if(a=="--pulse-normalization")c.pulseMeanNormalization=std::clamp(next("--pulse-normalization"),0.0,1.0);
        else if(a=="--mixer-exponent")c.mixerLevelExponent=std::clamp(next("--mixer-exponent"),0.1,4.0);
        else if(a=="--sub-gain")c.subGain=std::clamp(next("--sub-gain"),0.0,4.0);
        else if(a=="--noise-gain")c.noiseGain=std::clamp(next("--noise-gain"),0.0,4.0);
        else if(a=="--filter-scale")filterScale=std::clamp(next("--filter-scale"),0.02,64.0);
        else if(a=="--filter-base-hz")c.filterBaseHz=std::clamp(next("--filter-base-hz"),2.0,2000.0);
        else if(a=="--filter-slope")c.filterSemitonesPerRaw=std::clamp(next("--filter-slope"),0.05,2.0);else if(a=="--hp-filter-base-hz")c.highPassFilterBaseHz=std::clamp(next("--hp-filter-base-hz"),1.0,2000.0);else if(a=="--hp-filter-slope")c.highPassFilterSemitonesPerRaw=std::clamp(next("--hp-filter-slope"),0.05,2.0);else if(a=="--hp-filter-curve-exp")c.highPassFilterCurveExponent=std::clamp(next("--hp-filter-curve-exp"),0.0,6.0);else if(a=="--hp-filter-top-hz")c.highPassFilterTopHz=std::clamp(next("--hp-filter-top-hz"),1000.0,100000.0);
        else if(a=="--filter-curve-exp")c.filterCurveExponent=std::clamp(next("--filter-curve-exp"),0.05,5.0);
        else if(a=="--filter-top-hz")c.filterTopHz=std::clamp(next("--filter-top-hz"),1000.0,100000.0);
        else if(a=="--filter-knee-raw")c.filterKneeRaw=std::clamp(next("--filter-knee-raw"),0.0,164.0);
        else if(a=="--filter-high-slope")c.filterHighSemitonesPerRaw=std::clamp(next("--filter-high-slope"),0.0,3.0);
        else if(a=="--lp-res-scale")c.lowPassResonanceScale=std::clamp(next("--lp-res-scale"),0.0,2.0);
        else if(a=="--filter-env-octaves")c.filterEnvRangeOctaves=std::clamp(next("--filter-env-octaves"),1.0,24.0);
        else if(a=="--pm-pitch-octaves")c.polyModOscPitchRangeOctaves=std::clamp(next("--pm-pitch-octaves"),0.0,12.0);
        else if(a=="--pm-shape-range")c.polyModShapeRange=std::clamp(next("--pm-shape-range"),0.0,2.0);
        else if(a=="--pm-pw-range")c.polyModPulseWidthRange=std::clamp(next("--pm-pw-range"),0.0,0.5);
        else if(a=="--pm-filter-octaves")c.polyModFilterRangeOctaves=std::clamp(next("--pm-filter-octaves"),0.0,12.0);
        else if(a=="--sync-reset-phase")c.syncResetPhase=next("--sync-reset-phase");
        else if(a=="--sync-trigger-phase")c.syncTriggerPhase=next("--sync-trigger-phase");
        else if(a=="--sync-slope-correction")c.syncSlopeCorrection=true;
        else if(a=="--osc2-mixer-polarity")c.osc2MixerPolarity=next("--osc2-mixer-polarity")<0.0?-1.0:1.0;
        else if(a=="--oversampling"){if(i+1>=argc)throw std::runtime_error("missing value for --oversampling");oversampling=std::stoi(argv[++i]);if(oversampling!=1&&oversampling!=2&&oversampling!=4&&oversampling!=8)throw std::runtime_error("--oversampling must be 1, 2, 4, or 8");}
        else if(a=="--osc-only"){diagnostics.mix=p6::VoiceDiagnosticMix::OscillatorsOnly;diagnostics.bypassFilters=true;isolationDiagnosticsActive=true;}
        else if(a=="--osc1-only"){diagnostics.mix=p6::VoiceDiagnosticMix::Osc1Only;diagnostics.bypassFilters=true;isolationDiagnosticsActive=true;}
        else if(a=="--osc2-only"){diagnostics.mix=p6::VoiceDiagnosticMix::Osc2Only;diagnostics.bypassFilters=true;isolationDiagnosticsActive=true;}
        else if(a=="--sync-only"){diagnostics.mix=p6::VoiceDiagnosticMix::Osc1Only;diagnostics.bypassFilters=true;diagnostics.bypassPolyMod=true;diagnostics.forceSync=true;isolationDiagnosticsActive=true;}
        else if(a=="--filter-bypass"){diagnostics.bypassFilters=true;isolationDiagnosticsActive=true;}
        else if(a=="--poly-mod-bypass"){diagnostics.bypassPolyMod=true;isolationDiagnosticsActive=true;}
        else if(a=="--env-exponent") { double e=std::clamp(next("--env-exponent"),0.2,8.0); c.ampEnvelopeCurveExponent=e; c.filterEnvelopeCurveExponent=e; }
        else if(a=="--amp-env-exponent") c.ampEnvelopeCurveExponent=std::clamp(next("--amp-env-exponent"),0.2,8.0);
        else if(a=="--filter-env-exponent") c.filterEnvelopeCurveExponent=std::clamp(next("--filter-env-exponent"),0.2,8.0);
        else if(a=="--env-max-seconds"){double s=std::clamp(next("--env-max-seconds"),0.05,60.0);c.ampAttackMaxSeconds=c.ampDecayMaxSeconds=c.ampReleaseMaxSeconds=c.filterAttackMaxSeconds=c.filterDecayMaxSeconds=c.filterReleaseMaxSeconds=s;}
        else if(a=="--amp-env-max-seconds"){double s=std::clamp(next("--amp-env-max-seconds"),0.05,60.0);c.ampAttackMaxSeconds=c.ampDecayMaxSeconds=c.ampReleaseMaxSeconds=s;}
        else if(a=="--filter-env-max-seconds"){double s=std::clamp(next("--filter-env-max-seconds"),0.05,60.0);c.filterAttackMaxSeconds=c.filterDecayMaxSeconds=c.filterReleaseMaxSeconds=s;}
        else if(a=="--tail-seconds")tail=std::clamp(next("--tail-seconds"),0.0,30.0);else if(a=="--pre-roll-seconds")preRoll=std::clamp(next("--pre-roll-seconds"),0.0,60.0);
        else if(a=="--reference-a-v07"||a=="--reference-a-v08"||a=="--reference-a-v09"){}
        else if(a=="--vintage")c.slopMode=p6::SlopMode::Vintage; else throw std::runtime_error("unknown option: "+a);
    }
    c.filterBaseHz*=filterScale;
    auto stream=p6::decodeP6SysexStream(readBytes(argv[1])); if(stream.entries.empty()){std::cerr<<"no P6 program\n";return 3;}
    auto ev=readEvents(argv[2]); if(ev.empty())return 4;
    constexpr int sr=48000; p6::P6Engine engine;engine.prepare(sr,oversampling);engine.setVoiceDiagnostics(diagnostics);engine.loadPatch(stream.entries.front().patch,c);if(isolationDiagnosticsActive){engine.setDistortion(0.0);engine.setPanSpread(0.0);}for(std::size_t i=0;i<std::size_t(std::ceil(preRoll*sr));++i)(void)engine.process();
    const double total=ev.back().time+tail; const auto ns=std::size_t(std::ceil(total*sr)); std::vector<float> out;out.reserve(ns*2);std::size_t next=0;double peak=0;
    for(std::size_t i=0;i<ns;++i){double now=double(i)/sr;while(next<ev.size()&&ev[next].time<=now+0.5/sr){auto&e=ev[next++];if(e.on)engine.noteOn(e.note,e.velocity);else engine.noteOff(e.note);}auto s=engine.process();peak=std::max({peak,std::abs(s.left),std::abs(s.right)});out.push_back(float(s.left));out.push_back(float(s.right));}
    if(!writeFloatWav(argv[3],out,sr))return 5;std::cout<<"Rendered '"<<stream.entries.front().patch.name<<"'; peak="<<peak<<" preRoll="<<preRoll<<" filterScale="<<filterScale<<" filterSlope="<<c.filterSemitonesPerRaw<<" hpBase="<<c.highPassFilterBaseHz<<" hpSlope="<<c.highPassFilterSemitonesPerRaw<<" hpCurveExp="<<c.highPassFilterCurveExponent<<" hpTop="<<c.highPassFilterTopHz<<" filterCurveExp="<<c.filterCurveExponent<<" filterTopHz="<<c.filterTopHz<<" filterKneeRaw="<<c.filterKneeRaw<<" filterHighSlope="<<c.filterHighSemitonesPerRaw<<" pwInset="<<c.pulseWidthEndInset<<" pwExp="<<c.pulseWidthCurveExponent<<" pulseNorm="<<c.pulseMeanNormalization<<" mixerExp="<<c.mixerLevelExponent<<" subGain="<<c.subGain<<" noiseGain="<<c.noiseGain<<" pmPitchOct="<<c.polyModOscPitchRangeOctaves<<" pmShape="<<c.polyModShapeRange<<" pmPW="<<c.polyModPulseWidthRange<<" pmFilterOct="<<c.polyModFilterRangeOctaves<<" syncResetPhase="<<c.syncResetPhase<<" syncTriggerPhase="<<c.syncTriggerPhase<<" syncSlopeCorrection="<<c.syncSlopeCorrection<<" osc2MixerPolarity="<<c.osc2MixerPolarity<<" oversampling="<<oversampling<<" filterBypass="<<diagnostics.bypassFilters<<" polyModBypass="<<diagnostics.bypassPolyMod<<" forceSync="<<diagnostics.forceSync<<"\n";
}
