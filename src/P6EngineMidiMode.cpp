#include "p6/P6Engine.h"

namespace p6 {

void P6Engine::midiAllNotesOff()
{
    heldCount_ = 0;

    if (unison_) {
        bool anyActive = false;
        for (auto& slot : voices_)
            anyActive = anyActive || slot.voice.active();

        if (sustainPedal_ && anyActive) {
            unisonSustainHeld_ = true;
            for (auto& slot : voices_) {
                if (!slot.voice.active())
                    continue;
                slot.keyDown = false;
                slot.sustainHeld = true;
                slot.owner = 0;
            }
        } else {
            releaseUnison();
        }
        return;
    }

    for (auto& slot : voices_) {
        if (!slot.voice.active())
            continue;

        slot.keyDown = false;
        if (slot.owner == 0 && sustainPedal_) {
            slot.sustainHeld = true;
            continue;
        }

        slot.sustainHeld = false;
        slot.voice.noteOff();
    }
}

void P6Engine::midiAllSoundOff()
{
    heldCount_ = 0;
    unisonSustainHeld_ = false;

    for (auto& slot : voices_) {
        slot.keyDown = false;
        slot.sustainHeld = false;
        slot.owner = 0;
        slot.note = -1;
        slot.voice.silence();
    }

    // Emergency audible kill must also invalidate delay/reverb energy, but it
    // must not walk second-long buffers on the audio thread. The FX layer marks
    // all pre-panic ring-buffer samples invalid in bounded time instead.
    effects_.clearRealtimeState();
}

} // namespace p6
