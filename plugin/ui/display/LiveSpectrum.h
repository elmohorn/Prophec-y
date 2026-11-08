#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>

namespace p6plugin::ui {
inline constexpr std::size_t kLiveFrameSize=1024;
inline constexpr std::size_t kSpectrumBins=kLiveFrameSize/2+1;
struct LiveAudioFrame {
    std::array<float,kLiveFrameSize> left{},right{};
    double sampleRate{48000.0};
};
// Called on the UI thread. Stereo power is combined after transforming each
// channel, so opposite-polarity stereo does not disappear from the display.
inline std::array<float,kSpectrumBins> spectrumDb(const LiveAudioFrame& frame) noexcept
{
    using Complex=std::complex<float>;
    auto transform=[](const auto& samples) {
        std::array<Complex,kLiveFrameSize> data{};
        for(std::size_t i=0;i<kLiveFrameSize;++i) {
            const float window=.5f-.5f*std::cos(2.f*std::numbers::pi_v<float>*static_cast<float>(i)/static_cast<float>(kLiveFrameSize-1));
            data[i]=Complex{(std::isfinite(samples[i])?samples[i]:0.f)*window,0.f};
        }
        for(std::size_t i=1,j=0;i<kLiveFrameSize;++i) {
            std::size_t bit=kLiveFrameSize>>1;
            for(;j&bit;bit>>=1)j^=bit;
            j^=bit;if(i<j)std::swap(data[i],data[j]);
        }
        for(std::size_t length=2;length<=kLiveFrameSize;length<<=1) {
            const float angle=-2.f*std::numbers::pi_v<float>/static_cast<float>(length);
            const Complex root{std::cos(angle),std::sin(angle)};
            for(std::size_t start=0;start<kLiveFrameSize;start+=length) {
                Complex phase{1.f,0.f};
                for(std::size_t j=0;j<length/2;++j) {
                    const auto a=data[start+j],b=data[start+j+length/2]*phase;
                    data[start+j]=a+b;data[start+j+length/2]=a-b;phase*=root;
                }
            }
        }
        return data;
    };
    const auto left=transform(frame.left),right=transform(frame.right);
    std::array<float,kSpectrumBins> result{};
    for(std::size_t i=0;i<result.size();++i) {
        const float amplitude=std::sqrt(.5f*(std::norm(left[i])+std::norm(right[i])))*4.f/static_cast<float>(kLiveFrameSize);
        result[i]=std::max(-90.f,20.f*std::log10(std::max(amplitude,0.0000316228f)));
    }
    return result;
}
}
