#include "p6/NrpnCodec.h"
#include "p6/ParameterMapper.h"
#include "p6/ParameterRegistry.h"
#include "p6/ProgramLayout.h"
#include "p6/SysexCodec.h"
#include "p6/SysexStream.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <vector>

namespace {
void check(bool ok, const char* what) {
    if (!ok) { std::cerr << "FAIL: " << what << '\n'; std::exit(1); }
}
void near(double a, double b, const char* what) {
    check(std::abs(a-b) <= 1e-9 * std::max({1.0,std::abs(a),std::abs(b)}), what);
}
p6::P6PatchState basePatch() {
    p6::P6PatchState p;
    for (const auto& f:p6::serializedParameterLayout()) p.rawPayload[f.sysexOffset]=f.minimum;
    for (auto id:{6,77,82,109,143,144})
        p.rawPayload[p6::findSerializedByNrpn(id)->sysexOffset]=127;
    p6::populateRawParametersFromPayload(p); return p;
}
std::vector<double> snapshot(const p6::MappedPatch& m) {
    const auto& v=m.voice;
    return {v.osc1.semitones,v.osc2.semitones,v.osc2.fineCents,
        v.osc1.shape,v.osc2.shape,v.osc1.pulseWidth,v.osc2.pulseWidth,
        v.osc1.level,v.osc2.level,v.subLevel,v.noiseLevel,double(v.sync),
        double(v.osc2LowFrequency),double(v.osc2KeyTrack),double(v.glideMode),
        double(v.glideOn),v.glideSeconds,v.ampEnvAmount,double(v.ampVelocity),
        v.ampAttack,v.ampDecay,v.ampSustain,v.ampRelease,
        v.filterAttack,v.filterDecay,v.filterSustain,v.filterRelease,
        v.lowPass.cutoffControl,v.highPass.cutoffControl,
        v.lowPass.resonance,v.highPass.resonance,v.lowPass.keyTrack,v.highPass.keyTrack,
        double(v.lowPass.velocity),double(v.highPass.velocity),
        v.lowPass.envAmountOctaves,v.highPass.envAmountOctaves,
        v.polyMod.filterEnvAmount,v.polyMod.osc2Amount,
        v.polyMod.osc1FreqAmount,v.polyMod.osc1ShapeAmount,v.polyMod.osc1PulseWidthAmount,
        v.lowPass.polyModAmountOctaves,v.highPass.polyModAmountOctaves,
        m.lfo.rateHz,m.lfo.initialAmount,double(m.lfo.shape),double(m.lfo.clockSync),double(m.lfo.noiseMode),
        double(v.lfo.osc1Freq),double(v.lfo.osc2Freq),double(v.lfo.pulseWidth),
        double(v.lfo.amp),double(v.lfo.lowPass),double(v.lfo.highPass),
        v.aftertouch.amount,double(v.aftertouch.osc1Freq),double(v.aftertouch.osc2Freq),
        double(v.aftertouch.lowPass),double(v.aftertouch.highPass),double(v.aftertouch.amp),double(v.aftertouch.lfoAmount),
        m.panSpread,m.programVolume,m.distortion,m.slopAmount,double(m.unison),
        double(m.unisonMode),double(m.keyMode),double(m.chordMode),m.pitchBendRangeSemitones,
        double(m.arpOn),double(m.arpMode),double(m.arpRange),double(m.clockDivision),m.clockBpm};
}
void auditDomains() {
    const auto base=basePatch(); std::size_t legal=0, bytes=0;
    std::set<int> offsets,ids;
    for (const auto& f:p6::serializedParameterLayout()) {
        check(offsets.insert(f.sysexOffset).second,"duplicate SysEx offset");
        check(ids.insert(f.nrpn).second,"duplicate NRPN");
        const auto* registry=p6::findProgramParameter(f.nrpn);
        check(registry && registry->minimum==f.minimum && registry->maximum==f.maximum,"registry/layout range mismatch");
        for (unsigned value=f.minimum;value<=f.maximum;++value) {
            auto wire=base;
            p6::NrpnDecoder decoder;
            decoder.processCC(99,f.nrpn>>7); decoder.processCC(98,f.nrpn&127);
            decoder.processCC(6,value>>7); const auto update=decoder.processCC(38,value&127);
            check(update && update->parameter==f.nrpn && update->value==value,"14-bit NRPN transport");
            p6::applyRawNrpn(wire,*update);
            check(wire.rawPayload[f.sysexOffset]==value,"NRPN -> payload byte");
            const auto encoded=p6::SysexCodec::encodeEditBuffer(wire);
            const auto decoded=p6::SysexCodec::decode(encoded);
            check(decoded.ok && decoded.patch.rawPayload==wire.rawPayload,"SysEx byte roundtrip");
            check(snapshot(p6::mapPatchToDSP(wire))==snapshot(p6::mapPatchToDSP(decoded.patch)),"NRPN/SysEx DSP equivalence");
            ++legal;
        }
        for (unsigned value=0;value<256;++value) {
            auto imported=base,edited=base;
            imported.rawPayload[f.sysexOffset]=value;
            p6::populateRawParametersFromPayload(imported);
            const auto before=imported.rawPayload;
            p6::applyRawNrpn(edited,{f.nrpn,static_cast<std::uint16_t>(value)});
            for (const auto& c:{p6::MapperCalibration{},p6::referenceACandidateV09()}) {
                if(snapshot(p6::mapPatchToDSP(imported,c))!=snapshot(p6::mapPatchToDSP(edited,c))) {
                    std::cerr<<"domain mismatch NRPN="<<f.nrpn<<" byte="<<value<<'\n';
                    check(false,"import/edit clamping equivalence");
                }
            }
            check(imported.rawPayload==before,"mapper must preserve raw bytes"); ++bytes;
        }
    }
    std::cout<<"serialized_fields="<<ids.size()<<" legal_transport_roundtrips="<<legal
             <<" imported_byte_cases="<<bytes<<" profiles=2\n";
}
void auditVoiceControls() {
    std::size_t cases=0;
    auto nonlinear=p6::referenceACandidateV09(); nonlinear.filterCurveExponent=1.7;
    auto piecewise=p6::referenceACandidateV09(); piecewise.filterHighSemitonesPerRaw=.9;
    for (auto c:{p6::MapperCalibration{},p6::referenceACandidateV09(),nonlinear,piecewise})
    for (double sr:{44100.,48000.,96000.}) for (int os:{1,2,4,8}) {
        p6::P6Voice voice; voice.prepare(sr,7,os);
        auto v=p6::mapPatchToDSP(basePatch(),c).voice;
        for (int raw=0;raw<=164;++raw) {
            v.lowPass.cutoffControl=raw; v.highPass.cutoffControl=raw; voice.setParams(v); voice.noteOn(60);
            const auto sample=voice.process();
            near(sample.lowPassCutoffHz,p6::rawFilterCutoffHz(raw,sr*os,c),"cutoff helper/voice law parity");
            const double hpBase=c.highPassFilterBaseHz>0?c.highPassFilterBaseHz:c.filterBaseHz;
            const double hpSlope=c.highPassFilterSemitonesPerRaw>0?c.highPassFilterSemitonesPerRaw:c.filterSemitonesPerRaw;
            const double hpHz=c.highPassFilterCurveExponent>0 && c.highPassFilterTopHz>hpBase
                ? hpBase*std::pow(c.highPassFilterTopHz/hpBase,std::pow(raw/164.,c.highPassFilterCurveExponent))
                : hpBase*std::exp2(raw*hpSlope/12.);
            near(sample.highPassCutoffHz,std::clamp(hpHz,5.,sr*os*.45),"independent HP law and inheritance");
            check(std::isfinite(sample.dry),"finite audio across cutoff grid"); ++cases;
        }
    }
    // Independently calculated octave/semitone summer, including velocity on
    // direct EG only. Do not pre-clamp the static base before negative modulation.
    p6::VoiceParams v; v.filterBaseHz=100.; v.filterSemitonesPerRaw=1.;
    v.lowPass.cutoffControl=48.; v.lowPass.keyTrack=.5; v.lowPass.velocity=true;
    v.lowPass.envAmountOctaves=2.; v.lowPass.polyModAmountOctaves=3.;
    v.polyMod.filterEnvAmount=.5; v.filterAttack=0;v.filterDecay=0;v.filterSustain=1;
    v.lfo.lowPass=true;v.aftertouch.lowPass=true;v.aftertouch.amount=-.5;
    v.osc1.shape=1.;v.polyMod.osc1ShapeAmount=1.;
    v.osc1.pulseWidth=.9;v.polyMod.osc1PulseWidthAmount=.5;
    p6::P6Voice voice;voice.prepare(48000,1,1);voice.setParams(v);voice.noteOn(72,.25);
    const auto s=voice.process(.1,.5);
    near(s.filterEnvelope,1.,"zero attack EG");
    // key +.5oct, direct EG +.5oct, PM +1.5oct, LFO +.5oct,
    // pressure -.25*48 semitones = -1oct => total +2oct.
    near(s.lowPassCutoffHz,6400.,"octave/semitone summer and velocity routing");
    near(s.osc1Shape,1.,"positive shape modulation rails at full pulse");
    near(s.osc1PulseWidth,.96,"PW clamp follows modulation");
    v.polyMod.filterEnvAmount=-1.;voice.setParams(v);
    const auto negative=voice.process(.1,.5);
    near(negative.osc1Shape,0.,"negative shape modulation can unrail pulse");
    near(negative.osc1PulseWidth,.4,"negative PW modulation");
    v.lowPass.cutoffControl=164;v.lowPass.keyTrack=0;v.lowPass.velocity=false;
    v.lowPass.envAmountOctaves=-8;v.lowPass.polyModAmountOctaves=0;
    v.lfo.lowPass=false;v.aftertouch.lowPass=false;voice.setParams(v);
    near(voice.process().lowPassCutoffHz,100.*std::exp2(164./12.-8.),"modulate before final frequency clamp");
    // Direct and PM envelope routing are algebraically degenerate in the
    // current model. Protect the audit's finding, without asserting hardware truth.
    v.lowPass.cutoffControl=30;v.lowPass.envAmountOctaves=2;
    v.lowPass.polyModAmountOctaves=3;v.polyMod.filterEnvAmount=.5;
    voice.setParams(v);const auto combined=voice.process().lowPassCutoffHz;
    v.lowPass.envAmountOctaves=3.5;v.lowPass.polyModAmountOctaves=0;
    voice.setParams(v);near(voice.process().lowPassCutoffHz,combined,"linear EG/PM degeneracy");
    std::cout<<"voice_cutoff_grid_cases="<<cases<<" sample_rates=3 oversampling_factors=4 laws=4\n";
}
void auditEnvelopeTimes() {
    std::size_t cases=0;
    for(double maximum:{2.5,10.}) for(double exponent:{3.,3.5}) {
        double previous=0.;
        for(int raw=0;raw<=127;++raw) {
            const double time=p6::rawEnvelopeTime(raw,maximum,exponent);
            check(time>=previous && time>=.0005,"monotonic envelope time law");
            previous=time;++cases;
        }
        near(previous,maximum,"envelope maximum in seconds");
    }
    for(auto nrpn:{6,77,82,109,143,144}) for(int raw:{0,127,254}) {
        near(p6::signedCentered01(raw),double(raw-127)/127.,"bipolar center and endpoints");
        auto p=basePatch();p6::applyRawNrpn(p,{static_cast<std::uint16_t>(nrpn),static_cast<std::uint16_t>(raw)});
        const auto m=p6::mapPatchToDSP(p);
        double actual=0.,scale=1.;
        if(nrpn==6){actual=m.voice.osc2.fineCents;scale=50.;}
        if(nrpn==77){actual=m.voice.lowPass.envAmountOctaves;scale=p6::MapperCalibration{}.filterEnvRangeOctaves;}
        if(nrpn==82){actual=m.voice.highPass.envAmountOctaves;scale=p6::MapperCalibration{}.filterEnvRangeOctaves;}
        if(nrpn==109)actual=m.voice.aftertouch.amount;
        if(nrpn==143)actual=m.voice.polyMod.filterEnvAmount;
        if(nrpn==144)actual=m.voice.polyMod.osc2Amount;
        near(actual,p6::signedCentered01(raw)*scale,"mapped bipolar center and endpoints");
    }
    for(double sr:{44100.,48000.,96000.}) for(int os:{1,2,4,8}) {
        p6::P6Voice voice;voice.prepare(sr,1,os);p6::VoiceParams v;
        v.filterAttack=.02;v.filterDecay=.03;v.filterSustain=.6;v.filterRelease=.04;
        voice.setParams(v);voice.noteOn(60);
        int frames=0;double value=0;
        do {value=voice.process().filterEnvelope;++frames;} while(value<.999999 && frames<sr);
        check(std::abs(frames/sr-.02)<2./sr,"attack seconds invariant under oversampling");
        do {value=voice.process().filterEnvelope;++frames;} while(value>.6 && frames<sr);
        check(std::abs(frames/sr-.05)<3./sr,"decay seconds invariant under oversampling");
        voice.noteOff();frames=0;
        do {value=voice.process().filterEnvelope;++frames;} while(value>0 && frames<sr);
        check(std::abs(frames/sr-.04)<2./sr,"release seconds invariant under oversampling");
    }
    std::cout<<"envelope_mapping_cases="<<cases<<" bipolar_endpoint_cases=18 envelope_clock_cases=12\n";
}
void auditBank(const char* path, const char* fingerprint=nullptr) {
    std::ifstream file(path,std::ios::binary);check(bool(file),"open bank");
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    const auto bank=p6::decodeP6SysexStream(bytes);
    check(bank.errors.empty() && bank.skippedBytes==0 && !bank.entries.empty(),"clean bank stream");
    std::size_t invalid=0; std::vector<double> digest;
    for(const auto& entry:bank.entries) {
        const auto& p=entry.patch;
        const auto encoded=p6::SysexCodec::encodeProgramDump(p,p.bank,p.program);
        check(std::equal(encoded.begin(),encoded.end(),bytes.begin()+entry.byteOffset),"factory exact SysEx roundtrip");
        for(const auto& f:p6::serializedParameterLayout()) {
            const auto value=p.rawPayload[f.sysexOffset];
            if(value<f.minimum || value>f.maximum) {
                ++invalid;std::cout<<"out_of_range bank="<<p.bank<<" program="<<p.program<<" nrpn="<<f.nrpn<<" raw="<<int(value)<<'\n';
            }
        }
        for(const auto& c:{p6::MapperCalibration{},p6::referenceACandidateV09()}) {
            const auto values=snapshot(p6::mapPatchToDSP(p,c));
            for(auto x:values) {check(std::isfinite(x),"finite bank mapping");digest.push_back(x);}
        }
    }
    if(fingerprint) {
        std::ofstream out(fingerprint);check(bool(out),"open fingerprint");
        out<<std::hexfloat;for(double x:digest) out<<x<<'\n';
        check(bool(out),"write fingerprint");
    }
    // Fingerprint contains derived DSP controls, not SysEx payloads.
    std::cout<<"factory_programs="<<bank.entries.size()<<" out_of_range_fields="<<invalid<<" mapped_scalars="<<digest.size()<<'\n';
}
}
int main(int argc,char** argv) {
    if((argc==3 || argc==4) && std::string(argv[1])=="--bank") {auditBank(argv[2],argc==4?argv[3]:nullptr);return 0;}
    check(argc==1,"usage: p6_parameter_domain_audit [--bank file.syx [fingerprint.txt]]");
    auditDomains(); auditVoiceControls(); auditEnvelopeTimes();std::cout<<"parameter domain audit passed\n";
}
