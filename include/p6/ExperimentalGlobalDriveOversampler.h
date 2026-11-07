#pragma once

// Experimental, opt-in oversampling of the GLOBAL stereo distortion stage.
// Not exposed to the plugin/host. Default P6Engine path remains bit-identical.
// FIR interpolation + decimation both use linear-phase, 32*factor+1 taps,
// with an exact 32 HOST-sample delay at decimation phase zero.
// No allocation, locks, trigonometry or reinitialisation during process().
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace p6 {

class ExperimentalGlobalDriveOversampler {
public:
    static constexpr int kDelayHostSamples=32;
    static constexpr int kMaxFactor=4;
    static constexpr int kMaxTaps=32*kMaxFactor+1;
    static constexpr int kHostHistory=33;

    void prepare(double sampleRate, int factor) noexcept {
        factor_=(factor==2 || factor==4) ? factor : 1;
        length_=32*factor_+1;
        alpha_=std::exp(-2.*kPi*12./(std::max(8000.,sampleRate)*factor_));
        double sum{};
        const double fc=.45/factor_;
        for(int k=0;k<length_;++k) {
            const double x=k-16.*factor_;
            const double sinc=std::abs(x)<1.e-12
                 ? 2.*fc : std::sin(2.*kPi*fc*x)/(kPi*x);
            const double phase=2.*kPi*k/(length_-1.);
            const double window=.42-.5*std::cos(phase)+.08*std::cos(2.*phase);
            taps_[static_cast<std::size_t>(k)]=sinc*window;
            sum+=taps_[static_cast<std::size_t>(k)];
        }
        for(int k=0;k<length_;++k)
            taps_[static_cast<std::size_t>(k)]/=sum;
        reset();
    }

    void reset() noexcept {
        hostL_.fill(0.);hostR_.fill(0.);
        highL_.fill(0.);highR_.fill(0.);
        hostWrite_=highWrite_=0;
        dcL_=dcR_={};
    }

    [[nodiscard]] int factor() const noexcept {return factor_;}
    [[nodiscard]] int latencySamples() const noexcept {
        return factor_==1 ? 0 : kDelayHostSamples;
    }

    [[nodiscard]] std::array<double,2> process(double left,double right,
                                                double drive) noexcept {
        if(factor_==1) return {left,right}; // engine owns 1x historical path.
        hostL_[static_cast<std::size_t>(hostWrite_)]=left;
        hostR_[static_cast<std::size_t>(hostWrite_)]=right;
        std::array<double,2> out{};
        for(int phase=0;phase<factor_;++phase) {
            // Polyphase upsampling of the zero-stuffed host-rate stream:
            // F*h[phase + k*F], k=0...32. No high-rate zero buffer needed.
            double interpL{},interpR{};
            for(int k=0;phase+k*factor_<length_;++k) {
                const auto hostIndex=static_cast<std::size_t>(
                    (hostWrite_-k+kHostHistory)%kHostHistory);
                const double c=factor_*taps_[static_cast<std::size_t>(
                    phase+k*factor_)];
                interpL+=c*hostL_[hostIndex];
                interpR+=c*hostR_[hostIndex];
            }
            highL_[static_cast<std::size_t>(highWrite_)]=
                nonlinear(interpL,drive,dcL_);
            highR_[static_cast<std::size_t>(highWrite_)]=
                nonlinear(interpR,drive,dcR_);

            // Decimate at phase zero so the two FIR group delays sum to
            // exactly 32 host samples, not 32+(factor-1)/factor.
            if(phase==0) {
                for(int k=0;k<length_;++k) {
                    const auto idx=static_cast<std::size_t>(
                        (highWrite_-k+length_)%length_);
                    const double c=taps_[static_cast<std::size_t>(k)];
                    out[0]+=highL_[idx]*c;
                    out[1]+=highR_[idx]*c;
                }
            }
            highWrite_=(highWrite_+1)%length_;
        }
        hostWrite_=(hostWrite_+1)%kHostHistory;
        return out;
    }

private:
    struct DcState {double x{},y{};};
    static constexpr double kPi=3.1415926535897932384626433832795;
    [[nodiscard]] double nonlinear(double x,double drive,DcState& dc) noexcept {
        if(drive<=0.)return x;
        constexpr double knee=20., rail=2., asymmetry=.65;
        const double k=drive*knee;
        const double bias=x<0.?asymmetry:1.;
        const double shaped=x*(1.+k)/(1.+k*bias*std::abs(x)/rail);
        const double highpass=shaped-dc.x+alpha_*dc.y;
        dc.x=shaped;dc.y=highpass;
        return highpass;
    }

    int factor_{1};
    int length_{33};
    int hostWrite_{};
    int highWrite_{};
    double alpha_{.995};
    std::array<double,kMaxTaps> taps_{};
    std::array<double,kHostHistory> hostL_{},hostR_{};
    std::array<double,kMaxTaps> highL_{},highR_{};
    DcState dcL_{},dcR_{};
};

} // namespace p6
