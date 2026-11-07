#include "ui/ContextPreview.h"
#include "p6/ADSREnvelope.h"
#include "p6/PolyBLEPOscillator.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#define CHECK(x) do{if(!(x)){std::cerr<<"failed: " #x <<" line "<<__LINE__<<'\n';return 1;}}while(false)
int main()
{
    using namespace p6plugin::ui;
    PreviewSpec envelope{PreviewKind::Envelope,{.3,.4,.6,.2,.25}};
    const auto curve=makeContextPreview(envelope);
    CHECK(curve.valid&&!curve.truncated);CHECK(curve.points.size()<280);
    CHECK(std::abs(curve.attackEnd-.3)<2./kEnvelopePreviewSampleRate);
    CHECK(std::abs(curve.decayEnd-.7)<3./kEnvelopePreviewSampleRate);
    CHECK(std::abs(curve.noteOff-.95)<1.e-8);
    CHECK(curve.points.back().y==0.);
    // Independent 48 kHz core simulation against the 8 kHz UI preview at
    // matching times. The tolerance accounts for preview sample quantization.
    p6::ADSREnvelope reference;reference.prepare(48000);reference.setTimes(.3,.4,.2);reference.setSustain(.6);reference.noteOnFromZero();
    unsigned rendered=0;
    for(const auto& point:curve.points) {
        const auto target=static_cast<unsigned>(std::lround(point.x*48000));
        while(rendered<target){if(rendered==45600)reference.noteOff();reference.process();++rendered;}
        CHECK(std::abs(reference.value()-point.y)<.003);
    }
    const auto fast=makeContextPreview({PreviewKind::Envelope,{.0005,10.,.5,10.,.25}});
    CHECK(fast.valid);CHECK(std::any_of(fast.points.begin(),fast.points.end(),[](auto p){return p.y==1.;}));
    CHECK(makeContextPreview({PreviewKind::Envelope,{60.,60.,.5,60.,.25}}).truncated);
    CHECK(!makeContextPreview({PreviewKind::Envelope,{-1.,1.,.5,1.,.25}}).valid);
    CHECK(!makeContextPreview({PreviewKind::Envelope,{std::numeric_limits<double>::quiet_NaN(),1.,.5,1.,.25}}).valid);
    CHECK(std::abs(p6::PolyBLEPOscillator::nominalWaveform(.25,0.,.5))<1.e-8);
    CHECK(std::abs(p6::PolyBLEPOscillator::nominalWaveform(.5,0.,.5)-1.)<1.e-8);
    for(double shape:{0.,.25,.5,.75,1.})for(double duty:{.04,.25,.5,.96}) {
        const auto wave=makeContextPreview({PreviewKind::Oscillator,{shape,duty,1.,0.,0.}});
        CHECK(wave.valid&&wave.points.size()==257);
        // Compare phase shape with the actual free-running anti-aliased
        // oscillator away from corners, after its one-sample initial state.
        p6::PolyBLEPOscillator oscillator;oscillator.prepare(1000000);oscillator.setFrequency(1000);oscillator.setShape(shape);oscillator.setPulseWidth(duty);oscillator.reset();
        for(int n=0;n<1000;++n) {
            const double phase=oscillator.phase();const double actual=oscillator.process();
            const double expected=p6::PolyBLEPOscillator::nominalWaveform(phase,shape,duty);
            if(n>2&&std::abs(expected-p6::PolyBLEPOscillator::nominalWaveform(phase-.003,shape,duty))<.05&&std::abs(expected-p6::PolyBLEPOscillator::nominalWaveform(phase+.003,shape,duty))<.05)
                CHECK(std::abs(expected-actual)<.01);
        }
    }
    std::cout<<"Context previews: core ADSR timing/shape, peak preservation, bounded work and oscillator law verified\n";
}
