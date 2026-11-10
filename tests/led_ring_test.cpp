#include "ui/controls/SegmentedLedRing.h"
#include <iostream>
#include <limits>

#define CHECK(condition) do { if(!(condition)) { std::cerr << "failed: " #condition << '\n'; return 1; } } while(false)

int main()
{
    using p6plugin::ui::ledSegmentState;
    constexpr int count=p6plugin::ui::kLedRingSegments;
    for(int index=0;index<count;++index) {
        CHECK(!ledSegmentState(index,0.f).lit);
        CHECK(ledSegmentState(index,1.f).lit);
        CHECK(ledSegmentState(index,1.f).tip==(index==count-1));
        CHECK(!ledSegmentState(index,.5f,.5f).lit);
        CHECK(ledSegmentState(index,0.f,.5f).lit==(index<=12));
        CHECK(ledSegmentState(index,1.f,.5f).lit==(index>=12));
        CHECK(!ledSegmentState(index,std::numeric_limits<float>::quiet_NaN()).lit);
        CHECK(!ledSegmentState(index,-1.f).lit);
        CHECK(ledSegmentState(index,2.f).lit);
        CHECK(!ledSegmentState(index,.4f,.4f).lit);
        CHECK(ledSegmentState(index,1.f,.4f).lit==(index>=10));
    }
    for(int step=1;step<=1000;++step) {
        const float position=static_cast<float>(step)/1000.f;
        int tips=0,lit=0;
        for(int index=0;index<count;++index) {
            const auto state=ledSegmentState(index,position);
            tips+=state.tip;lit+=state.lit;
        }
        CHECK(tips==1&&lit>=1&&lit<=count);
    }
    std::cout << "LED ring: bounded positions, extrema, bipolar zero and tip verified\n";
}
