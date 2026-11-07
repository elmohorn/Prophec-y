// Isolated global distortion aliasing experiment. NEVER changes the engine DSP.
// The *actual* P6Engine distortion stage is exercised through its pre-FX
// injection callback; no synthetic approximation of its nonlinear equation.
// Compare ideal-tone upper bounds with offline FIR-reconstructed *sampled*
// inputs. Reconstruction is zero-phase/noncausal in this probe and is NOT
// a production-ready realtime oversampler; implementation requires PDC and
// bypass alignment.
#include "p6/P6Engine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
constexpr double pi = 3.1415926535897932384626433832795;
constexpr int fftLength = 4096;
constexpr int warmup = 512;
constexpr int fundamentalBin = 863; // 0.2107 * Fs; h3+ alias into audio band.
constexpr double fundamentalFraction = double(fundamentalBin) / fftLength;

struct Injection {
    double phase{};
    double increment{};
    double amplitude{0.35};
    const std::vector<double>* reconstructed{};
    std::size_t index{};
    static p6::StereoSample emit(void* ctx, p6::StereoSample) noexcept {
        auto& self=*static_cast<Injection*>(ctx);
        const double v=self.reconstructed
            ? (*self.reconstructed)[self.index++]
            : self.amplitude * std::sin(self.phase);
        self.phase+=self.increment;
        if(self.phase>=2.*pi) self.phase-=2.*pi;
        return {v, v};
    }
};

std::vector<double> decimator(int factor) {
    if(factor==1) return {1.};
    const int taps=32*factor+1;
    const double fc=.45/static_cast<double>(factor);
    const double mid=(taps-1)*.5;
    std::vector<double> kernel(static_cast<std::size_t>(taps));
    double sum{};
    for(int k=0;k<taps;++k) {
        const double x=k-mid;
        const double sinc=std::abs(x)<1.e-12?2.*fc:std::sin(2.*pi*fc*x)/(pi*x);
        const double phase=2.*pi*k/double(taps-1);
        const double window=.42-.5*std::cos(phase)+.08*std::cos(2.*phase);
        kernel[static_cast<std::size_t>(k)]=sinc*window;
        sum+=sinc*window;
    }
    if(std::abs(sum)<1.e-12)throw std::logic_error("decimator sum zero");
    for(auto& t:kernel)t/=sum;
    return kernel;
}

// Offline windowed-sinc reconstruction of samples taken at the HOST rate.
// Uses 33 host-sample support and polyphase coefficients. It is noncausal
// here so a deployable FIR needs delay compensation (~16 input samples plus
// ~16 input samples for post-filter). Fundamental tone is never synthesized
// directly at the oversampled rate for this path.
std::vector<double> interpolateHostTone(int count,int factor) {
    constexpr int half=16;
    constexpr double amp=.35;
    std::vector<double> host(static_cast<std::size_t>(count+2*half));
    for(int k=-half;k<count+half;++k)
        host[static_cast<std::size_t>(k+half)] =
            amp*std::sin(2.*pi*fundamentalFraction*double(k));
    std::vector<std::array<double,2*half+1>> weights(
        static_cast<std::size_t>(factor));
    for(int phase=0;phase<factor;++phase) {
        const double fractional=double(phase)/factor;
        double normalization{};
        for(int offset=-half;offset<=half;++offset){
            const double x=fractional-offset;
            const double w=std::abs(x)<=half
              ? .42+.5*std::cos(pi*x/half)+.08*std::cos(2.*pi*x/half)
              : 0.;
            const double sinc=std::abs(x)<1.e-12
              ? 1. : std::sin(pi*x)/(pi*x);
            const double value=w*sinc;
            weights[static_cast<std::size_t>(phase)]
                   [static_cast<std::size_t>(offset+half)]=value;
            normalization+=value;
        }
        for(auto& weight:weights[static_cast<std::size_t>(phase)])
            weight/=normalization;
    }
    std::vector<double> high(static_cast<std::size_t>(count*factor));
    for(int i=0;i<count*factor;++i) {
        const int center=i/factor, phase=i%factor;
        double reconstructed{};
        for(int offset=-half;offset<=half;++offset)
            reconstructed+=host[static_cast<std::size_t>(center+offset+half)]*
               weights[static_cast<std::size_t>(phase)]
                      [static_cast<std::size_t>(offset+half)];
        high[static_cast<std::size_t>(i)]=reconstructed;
    }
    return high;
}

std::vector<double> render(double fs, int factor, double amount,
                           bool hostReconstructed=false) {
    p6::P6Engine engine;
    engine.prepare(fs*factor, 1); // voice path silent; only global distortion used.
    engine.setDistortion(amount); // after prepare; effects default disabled.
    Injection injection;
    injection.increment=2.*pi*fundamentalFraction/double(factor);
    const int n=warmup+fftLength;
    std::vector<double> interpolated;
    if(hostReconstructed && factor>1) {
        interpolated=interpolateHostTone(n,factor);
        injection.reconstructed=&interpolated;
    }
    engine.setCharacterInsert(&injection, &Injection::emit, true);
    std::vector<double> high(static_cast<std::size_t>(n*factor));
    for(auto& value:high)value=engine.process().left;
    const auto fir=decimator(factor);
    std::vector<double> result(static_cast<std::size_t>(fftLength));
    for(int i=0;i<fftLength;++i){
        const auto base=static_cast<std::size_t>((warmup+i)*factor + factor-1);
        double y{};
        for(std::size_t k=0;k<fir.size();++k) {
            if(base>=k) y+=fir[k]*high[base-k];
        }
        result[static_cast<std::size_t>(i)]=y;
    }
    return result;
}

double binAmplitude(const std::vector<double>& data, int bin) {
    const double phase=-2.*pi*double(bin)/double(data.size());
    const std::complex<double> rotation{std::cos(phase),std::sin(phase)};
    std::complex<double> phasor{1.,0.}, sum{0.,0.};
    for(const auto sample:data){sum+=sample*phasor;phasor*=rotation;}
    return 2.*std::abs(sum)/double(data.size());
}

struct Metrics {
    double fundamental{};
    double aliasRms{};
    double aliasDbc{};
    double peak{};
};

Metrics metrics(const std::vector<double>& data) {
    Metrics m{};
    m.fundamental=binAmplitude(data,fundamentalBin);
    double aliasSquare{};
    for(int h=2;h<=11;++h) {
        const int unwrapped=h*fundamentalBin;
        if(unwrapped<=fftLength/2)continue; // legitimate in-band harmonics.
        const int folded=unwrapped%fftLength;
        const int bin=std::min(folded,fftLength-folded);
        if(bin==0 || bin==fundamentalBin)continue;
        const double amp=binAmplitude(data,bin);
        aliasSquare+=amp*amp;
    }
    m.aliasRms=std::sqrt(aliasSquare/2.);
    m.aliasDbc=20.*std::log10(std::max(1.e-18,std::sqrt(aliasSquare))/
                             std::max(1.e-18,m.fundamental));
    for(double y:data)m.peak=std::max(m.peak,std::abs(y));
    return m;
}

struct Row {double fs{},amount{};int factor{};bool reconstructed{};Metrics m;};

bool verify(const std::vector<Row>& rows) {
    bool valid=true;
    for(double fs:{44100.,48000.,96000.})for(double drive:{.45,.9}){
        double original=std::numeric_limits<double>::quiet_NaN();
        double ideal4x=std::numeric_limits<double>::quiet_NaN();
        double fir4x=std::numeric_limits<double>::quiet_NaN();
        for(const auto& row:rows) {
            if(row.fs!=fs || row.amount!=drive)continue;
            if(row.factor==1)original=row.m.aliasDbc;
            if(row.factor==4) {
                if(row.reconstructed)fir4x=row.m.aliasDbc;
                else ideal4x=row.m.aliasDbc;
            }
        }
        // Require sensitivity and a genuine improvement in the reconstructed
        // (not only ideal source) experiment; never assert a production claim.
        valid &= std::isfinite(original) && original>-130.;
        valid &= std::isfinite(ideal4x) && ideal4x<original-3.;
        valid &= std::isfinite(fir4x) && fir4x<original-3.;
    }
    return valid;
}
} // namespace

int main(int argc,char** argv) {
    const bool selfTest=argc==2 && std::string_view(argv[1])=="--self-test";
    if(argc!=1 && !selfTest) {
        std::cerr<<"usage: p6_global_distortion_alias_probe [--self-test]\n";
        return 2;
    }
    std::vector<Row> rows;
    std::cout<<"sample_rate_hz,drive,oversampling_factor,input_source,fundamental_amp,folded_alias_rms,folded_alias_dbc,peak\n";
    std::cout<<std::fixed<<std::setprecision(6);
    for(const double fs:{44100.,48000.,96000.})
        for(const double drive:{.45,.9})
            for(const int factor:{1,2,4})
                for(const bool reconstructed:{false,true}) {
                    if(factor==1 && reconstructed)continue;
                    const auto m=metrics(render(fs,factor,drive,reconstructed));
                    rows.push_back({fs,drive,factor,reconstructed,m});
                    std::cout<<fs<<','<<drive<<','<<factor<<','
                             <<(reconstructed?"host_fir":"ideal_tone")<<','
                             <<m.fundamental<<','<<m.aliasRms<<','
                             <<m.aliasDbc<<','<<m.peak<<'\n';
                }
    if(selfTest && !verify(rows)) {
        std::cerr<<"Alias diagnostic sensitivity or 4x upper-bound reduction failed\n";
        return 1;
    }
    return 0;
}
