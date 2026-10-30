#include "p6/P6PerformanceScheduler.h"

namespace p6 {

void P6PerformanceScheduler::midiAllNotesOff()
{
    // Channel-mode All Notes Off clears the physical/arp input set and releases
    // generated owners normally. Live owner-0 voices are then released through
    // the engine's sustain-aware path.
    arpeggiator_.clear();
    releaseArpNote();
    releaseSequenceNotes();
    engine_.midiAllNotesOff();
}

void P6PerformanceScheduler::midiAllSoundOff()
{
    // Immediate silence must not be implemented as a run of ordinary Note Off
    // messages because release envelopes and Damper would keep sounding. Clear
    // scheduler ownership bookkeeping and let the engine kill the audible
    // envelopes without resetting its free-running oscillator state.
    arpeggiator_.clear();
    activeArpNote_ = -1;
    activeSequenceCount_ = 0;
    engine_.midiAllSoundOff();
}

} // namespace p6
