#include "p6/P6Voice.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void w16(std::ofstream& f, std::uint16_t v){ const char b[2]={char(v&255),char((v>>8)&255)}; f.write(b,2); }
void w32(std::ofstream& f, std::uint32_t v){ const char b[4]={char(v&255),char((v>>8)&255),char((v>>16)&255),char((v>>24)&255)}; f.write(b,4); }
bool writeFloatWav(const std::string& path,const std::vector<float>& x,int sr){
    std::ofstream f(path,std::ios::binary); if(!f)return false; const auto bytes=std::uint32_t(x.size()*sizeof(float));
    f.write("RIFF",4);w32(f,36+bytes);f.write("WAVE",4);f.write("fmt ",4);w32(f,16);w16(f,3);w16(f,2);
    w32(f,sr);w32(f,sr*8);w16(f,8);w16(f,32);f.write("data",4);w32(f,bytes);f.write(reinterpret_cast<const char*>(x.data()),std::streamsize(bytes));return bool(f);
}
int checkedOS(int x){if(x!=1&&x!=2&&x!=4&&x!=8)throw std::runtime_error("oversampling must be 1, 2, 4, or 8");return x;}
}

int main(int argc,char**argv){
    if(argc<2){std::cerr<<"usage: p6_polymod_probe <out.wav> [--destination pitch|shape|pw|lp|hp] [--note 48] [--osc2-semis 12] [--source-amount 1] [--destination-amount N] [--oversampling 1|2|4|8] [--seconds N] [--seed N]\n";return 2;}
    std::string destination="pitch"; int note=48,os=2; double osc2Semis=12.0,sourceAmount=1.0,destAmount=-1.0,seconds=2.0; std::uint32_t seed=0x9901u;
    for(int i=2;i<argc;++i){std::string a=argv[i];auto next=[&](const char*o){if(i+1>=argc)throw std::runtime_error(std::string("missing value for ")+o);return std::string(argv[++i]);};
        if(a=="--destination")destination=next("--destination");else if(a=="--note")note=std::clamp(std::stoi(next("--note")),0,127);else if(a=="--osc2-semis")osc2Semis=std::stod(next("--osc2-semis"));
        else if(a=="--source-amount")sourceAmount=std::clamp(std::stod(next("--source-amount")),-1.0,1.0);else if(a=="--destination-amount")destAmount=std::stod(next("--destination-amount"));
        else if(a=="--oversampling")os=checkedOS(std::stoi(next("--oversampling")));else if(a=="--seconds")seconds=std::clamp(std::stod(next("--seconds")),0.1,30.0);else if(a=="--seed")seed=std::uint32_t(std::stoul(next("--seed"),nullptr,0));else throw std::runtime_error("unknown option: "+a);}
    if(destination!="pitch"&&destination!="shape"&&destination!="pw"&&destination!="lp"&&destination!="hp")throw std::runtime_error("destination must be pitch, shape, pw, lp, or hp");

    p6::VoiceParams p; p.osc1.shape=0.5;p.osc1.pulseWidth=0.5;p.osc1.level=1.0;p.osc2.shape=0.5;p.osc2.pulseWidth=0.5;p.osc2.level=0.0;p.osc2.semitones=osc2Semis;p.subLevel=0;p.noiseLevel=0;p.sync=false;
    p.polyMod.filterEnvAmount=0.0;p.polyMod.osc2Amount=sourceAmount;
    if(destination=="pitch")p.polyMod.osc1FreqAmount=destAmount>=0?destAmount:4.0;
    else if(destination=="shape")p.polyMod.osc1ShapeAmount=destAmount>=0?destAmount:1.0;
    else if(destination=="pw")p.polyMod.osc1PulseWidthAmount=destAmount>=0?destAmount:0.5;
    else if(destination=="lp"){p.lowPass.polyModAmountOctaves=destAmount>=0?destAmount:7.0;p.lowPass.cutoffControl=80.0;p.lowPass.resonance=0.15;p.highPass.cutoffControl=0.0;}
    else {p.highPass.polyModAmountOctaves=destAmount>=0?destAmount:7.0;p.highPass.cutoffControl=58.0;p.highPass.resonance=0.15;p.lowPass.cutoffControl=164.0;}
    p.ampAttack=0;p.ampDecay=0.001;p.ampSustain=1;p.ampRelease=0.001;p.filterAttack=0;p.filterDecay=0.001;p.filterSustain=0;p.filterRelease=0.001;

    p6::VoiceDiagnostics d;d.mix=p6::VoiceDiagnosticMix::Osc1Only;d.bypassPolyMod=false;d.bypassFilters=(destination!="lp"&&destination!="hp");
    p6::P6Voice voice;voice.prepare(48000.0,seed,os);voice.setParams(p);voice.setDiagnostics(d);voice.noteOn(note,1.0);
    for(int i=0;i<12000;++i)(void)voice.process();
    const auto n=std::size_t(std::ceil(seconds*48000.0));std::vector<float> audio;audio.reserve(n*2);double peak=0;
    for(std::size_t i=0;i<n;++i){const auto y=voice.process().dry;peak=std::max(peak,std::abs(y));audio.push_back(float(y));audio.push_back(float(y));}
    if(!writeFloatWav(argv[1],audio,48000))return 3;
    std::cout<<"poly-mod probe: destination="<<destination<<" note="<<note<<" osc2Semis="<<osc2Semis<<" source="<<sourceAmount<<" oversampling="<<os<<" peak="<<peak<<"\n";
}
