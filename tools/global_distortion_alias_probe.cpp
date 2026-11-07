// Isolated global distortion aliasing experiment. NEVER changes the engine DSP.
// The *actual* P6Engine distortion stage is exercised through its pre-FX
// injection callback; no synthetic approximation of its nonlinear equation.
// The 2x/4x signals are IDEAL-TONE upper bounds: for a deployed wrapper a
// practical anti-imaging interpolator, latency alignment and A/B remain needed.
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
    static p6::StereoSample emit(void* ctx, p6::StereoSample) noexcept {
        auto& self=*static_cast<Injection*>(ctx);
        const double v=self.amplitude * std::sin(self.phase);
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

std::vector<double> render(double fs, int factor, double amount) {
    p6::P6Engine engine;
    engine.prepare(fs*factor, 1); // voice path silent; only global distortion used.
    engine.setDistortion(amount); // after prepare; effects default disabled.
    Injection injection;
    injection.increment=2.*pi*fundamentalFraction/double(factor);
    engine.setCharacterInsert(&injection, &Injection::emit, true);
    const int n=warmup+fftLength;
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

struct Row {double fs{},amount{};int factor{};Metrics m;};

bool verify(const std::vector<Row>& rows) {
    bool valid=true;
    for(double fs:{44100.,48000.,96000.})for(double drive:{.45,.9}){
        std::array<Metrics,3> m{};
        for(const auto& row:rows){
            if(row.fs!=fs || row.amount!=drive)continue;
            m[static_cast<std::size_t>(row.factor==1?0:row.factor==2?1:2)]=row.m;
        }
        // The actual base-rate distortion is nonlinear; if no alias is
        // observed the experiment has no sensitivity and must fail.
        if(!std::isfinite(m[0].aliasDbc)||m[0].aliasDbc < -130.)valid=false;
        // This is an *ideal input-tone* upper bound, not permission to ship
        // FIR-based oversampling without considering PDC and phase shifts.
        if(!(m[2].aliasDbc < m[0].aliasDbc-3.))valid=false;
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
    std::cout<<"sample_rate_hz,drive,ideal_tone_factor,fundamental_amp,folded_alias_rms,folded_alias_dbc,peak\n";
    std::cout<<std::fixed<<std::setprecision(6);
    for(const double fs:{44100.,48000.,96000.})
        for(const double drive:{.45,.9})
            for(const int factor:{1,2,4}) {
                const auto m=metrics(render(fs,factor,drive));
                rows.push_back({fs,drive,factor,m});
                std::cout<<fs<<','<<drive<<','<<factor<<','
                         <<m.fundamental<<','<<m.aliasRms<<','<<m.aliasDbc<<','<<m.peak<<'\n';
            }
    if(selfTest && !verify(rows)) {
        std::cerr<<"Alias diagnostic sensitivity or 4x upper-bound reduction failed\n";
        return 1;
    }
    return 0;
}
