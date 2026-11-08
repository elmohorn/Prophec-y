#include "ui/display/LiveSpectrum.h"
#include "RealtimeSnapshotMailbox.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

int main()
{
    using namespace p6plugin::ui;
    LiveAudioFrame frame{};
    auto quiet=spectrumDb(frame);
    if(*std::max_element(quiet.begin(),quiet.end())>-89.f)return 1;
    constexpr std::size_t bin=32;
    for(std::size_t i=0;i<kLiveFrameSize;++i) {
        frame.left[i]=std::sin(2.f*std::numbers::pi_v<float>*static_cast<float>(bin*i)/static_cast<float>(kLiveFrameSize));
        frame.right[i]=-frame.left[i];
    }
    const auto spectrum=spectrumDb(frame);
    const auto peak=static_cast<std::size_t>(std::max_element(spectrum.begin(),spectrum.end())-spectrum.begin());
    if(peak!=bin || spectrum[bin]<-1.f || spectrum[bin]>1.f)return 2;
    frame.left[0]=std::numeric_limits<float>::quiet_NaN();
    for(float v:spectrumDb(frame))if(!std::isfinite(v))return 3;
    p6plugin::RealtimeSnapshotMailbox<LiveAudioFrame> mailbox;
    LiveAudioFrame copy{};std::uint32_t revision{};
    if(mailbox.tryRead(copy,revision) || !mailbox.publish(frame) || !mailbox.tryRead(copy,revision) || revision==0)return 4;
    if(copy.right[bin]!=frame.right[bin])return 5;
    std::cout<<"Live stereo spectrum: silence, frequency, antiphase and frame handoff verified\n";
}
