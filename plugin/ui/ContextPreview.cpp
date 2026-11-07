#include "ContextPreview.h"
#include "p6/ADSREnvelope.h"
#include "p6/PolyBLEPOscillator.h"
#include <algorithm>
#include <cmath>

namespace p6plugin::ui {
PreviewCurve makeContextPreview(const PreviewSpec& spec)
{
    PreviewCurve result;
    if(!std::all_of(spec.values.begin(),spec.values.end(),[](double x){return std::isfinite(x);}))return result;
    if(spec.kind==PreviewKind::Oscillator) {
        result.points.reserve(257);
        for(int i=0;i<=256;++i) {
            const double phase=static_cast<double>(i)/256.;
            const double value=p6::PolyBLEPOscillator::nominalWaveform(phase,spec.values[0],spec.values[1],spec.values[2]);
            result.points.push_back({phase,value});result.amplitude=std::max(result.amplitude,std::abs(value));
        }
        result.valid=true;return result;
    }
    if(spec.kind!=PreviewKind::Envelope)return result;
    if(spec.values[0]<0.||spec.values[1]<0.||spec.values[3]<0.||spec.values[4]<0.)return result;
    const double duration=spec.values[0]+spec.values[1]+spec.values[3]+spec.values[4];
    constexpr unsigned budget=260000; // Bounded UI work even for future profiles.
    const double requested=std::ceil(duration*kEnvelopePreviewSampleRate)+2.;
    const auto frames=static_cast<unsigned>(std::clamp(requested,2.,static_cast<double>(budget)));
    result.truncated=requested>budget;result.duration=frames/kEnvelopePreviewSampleRate;
    const double gate=std::ceil((spec.values[0]+spec.values[1]+spec.values[4])*kEnvelopePreviewSampleRate);
    result.noteOff=gate/kEnvelopePreviewSampleRate;
    const unsigned stride=std::max(1u,frames/256u);
    p6::ADSREnvelope envelope;envelope.prepare(kEnvelopePreviewSampleRate);
    envelope.setTimes(spec.values[0],spec.values[1],spec.values[3]);envelope.setSustain(spec.values[2]);
    envelope.noteOnFromZero();result.points.reserve(280);result.points.push_back({0.,0.});
    for(unsigned i=1;i<=frames;++i) {
        if(static_cast<double>(i-1)==gate) {
            result.points.push_back({result.noteOff,envelope.value()});envelope.noteOff();
        }
        const auto before=envelope.stage();const double value=envelope.process();const auto after=envelope.stage();
        const double time=i/kEnvelopePreviewSampleRate;
        if(before==p6::ADSREnvelope::Stage::Attack&&after!=before)result.attackEnd=time;
        if(before==p6::ADSREnvelope::Stage::Decay&&after!=before)result.decayEnd=time;
        // Preserve stage transitions as well as the regular grid, so a very
        // short attack peak is not lost on a long time axis.
        if(i%stride==0||before!=after||i==frames)result.points.push_back({time,value});
    }
    result.valid=true;return result;
}
} // namespace p6plugin::ui
