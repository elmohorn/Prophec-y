#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace p6plugin {
inline constexpr const char* kMasterOutputID="wrapper_master_output_db";
inline constexpr float kNewMasterDb=-18.f;
struct OutputReadings {
    std::array<float,2> peak{},rms{},prePeak{};
    unsigned clips{}; // bits 0/1 final L/R, 2/3 pre-master L/R
};
// Allocation-free final attenuation, after every synth/FX route. Meter levels
// are sample peaks and a 50 ms exponentially weighted RMS, not true-peak.
class OutputStage {
public:
    void prepare(double rate,float db) noexcept {
        rate_=std::max(1.,rate);rmsPole_=std::exp(-1./(.05*rate_));
        rampLength_=std::max(1,static_cast<int>(rate_*.02));
        current_=target_=gain(db);remaining_=0;energy_={};beginBlock();
    }
    void setDecibels(float db) noexcept {
        const float next=gain(db);
        if(next==target_)return;
        target_=next;remaining_=rampLength_;step_=(target_-current_)/static_cast<float>(remaining_);
    }
    void beginBlock() noexcept { readings_={}; }
    std::array<float,2> process(float left,float right) noexcept {
        if(remaining_>0){current_+=step_;if(--remaining_==0)current_=target_;}
        std::array<float,2> out{left*current_,right*current_};
        const std::array<float,2> in{left,right};
        for(std::size_t c=0;c<2;++c) {
            const float pre=std::abs(in[c]),post=std::abs(out[c]);
            readings_.prePeak[c]=std::max(readings_.prePeak[c],pre);
            readings_.peak[c]=std::max(readings_.peak[c],post);
            if(post>=1.f)readings_.clips|=1u<<c;
            if(pre>=1.f)readings_.clips|=4u<<c;
            energy_[c]=rmsPole_*energy_[c]+(1.-rmsPole_)*static_cast<double>(out[c])*out[c];
        }
        return out;
    }
    OutputReadings readings() const noexcept {
        auto result=readings_;for(std::size_t c=0;c<2;++c)result.rms[c]=static_cast<float>(std::sqrt(energy_[c]));return result;
    }
private:
    static float gain(float db) noexcept {return std::pow(10.f,(std::isfinite(db)?std::clamp(db,-60.f,0.f):0.f)/20.f);}
    double rate_{48000},rmsPole_{};
    std::array<double,2> energy_{};
    float current_{1},target_{1},step_{};
    int remaining_{},rampLength_{960};
    OutputReadings readings_{};
};
} // namespace p6plugin
