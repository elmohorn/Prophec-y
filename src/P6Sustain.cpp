#include "p6/P6Engine.h"

#include <algorithm>

namespace p6 {

void P6Engine::setSustainPedal(bool enabled)
{
    if (sustainPedal_ == enabled)
        return;

    sustainPedal_ = enabled;
    if (enabled)
        return;

    // Release only the exact voice incarnation that was deferred. A voice may
    // have been stolen while the pedal was down; age changes on every new
    // allocation/retarget, so a stale marker can never terminate that new note.
    for (std::size_t i = 0; i < voices_.size(); ++i) {
        auto& slot = voices_[i];
        const auto deferredAge = sustainedVoiceAge_[i];
        if (deferredAge != 0 && slot.age == deferredAge && !slot.keyDown && slot.voice.active())
            slot.voice.noteOff();
    }
    sustainedVoiceAge_.fill(0);
}

void P6Engine::noteOffOwnedSustainAware(int midiNote, std::uint32_t owner)
{
    if (!sustainPedal_) {
        noteOffOwned(midiNote, owner);
        return;
    }

    midiNote = std::clamp(midiNote, 0, 127);

    if (unison_) {
        // Physical/key-priority ownership changes immediately even though the
        // envelopes are still held by the pedal. This is what keeps Damper
        // separate from Arp Hold and from chord-memory capture.
        forgetHeld(midiNote);
        if (heldCount_ > 0) {
            retargetUnison(false);
            return;
        }

        for (std::size_t i = 0; i < voices_.size(); ++i) {
            auto& slot = voices_[i];
            if (!slot.voice.active())
                continue;
            slot.keyDown = false;
            sustainedVoiceAge_[i] = slot.age;
        }
        return;
    }

    if (owner == 0)
        forgetHeld(midiNote);

    for (std::size_t i = 0; i < voices_.size(); ++i) {
        auto& slot = voices_[i];
        if (slot.keyDown && slot.note == midiNote && slot.owner == owner) {
            slot.keyDown = false;
            sustainedVoiceAge_[i] = slot.age;
            // Deliberately do not call P6Voice::noteOff(): CC64 >= 64 holds the
            // envelopes at sustain until the pedal is released.
        }
    }
}

std::size_t P6Engine::sustainedVoiceCount() const noexcept
{
    std::size_t count = 0;
    for (std::size_t i = 0; i < voices_.size(); ++i) {
        const auto& slot = voices_[i];
        if (sustainedVoiceAge_[i] != 0
            && sustainedVoiceAge_[i] == slot.age
            && !slot.keyDown
            && slot.voice.active())
            ++count;
    }
    return count;
}

} // namespace p6
