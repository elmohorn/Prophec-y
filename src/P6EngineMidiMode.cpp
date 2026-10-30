#include "p6/P6Engine.h"

namespace p6 {

void P6Engine::midiAllNotesOff()
{
    // MIDI All Notes Off is equivalent to releasing the currently held keys.
    // Damper remains authoritative: physically held bookkeeping is cleared,
    // but owner-0 voices whose release is deferred by CC64 keep sounding.
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
    // CC120 is the emergency audible kill. It does not reset controllers and
    // it must not use P6Voice::reset(), because that would also restart the
    // Prophet-style free-running VCO/slop state.
    heldCount_ = 0;
    unisonSustainHeld_ = false;

    for (auto& slot : voices_) {
        slot.keyDown = false;
        slot.sustainHeld = false;
        slot.owner = 0;
        slot.note = -1;
        slot.voice.silence();
    }

    // Once global effects exist, "All Sound Off" must include their stored
    // energy too. Otherwise a delay/reverb tail would remain audible after the
    // emergency kill even though every voice had been silenced. reset() only
    // clears preallocated DSP state; it does not allocate or change controllers.
    effects_.reset();
}

} // namespace p6
