#include "p6/P6MidiRouter.h"

#include "p6/P6Sequence.h"
#include "p6/ParameterRegistry.h"
#include "p6/ProgramLayout.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace p6 {
namespace {

bool isSupportedProgramNrpn(std::uint16_t nrpn)
{
    if (nrpn >= 236 && nrpn <= 255)
        return true;
    SequenceNrpnAddress address{};
    if (sequenceAddressForNrpn(nrpn, address))
        return true;
    return findProgramParameter(nrpn) != nullptr;
}

} // namespace

int P6MidiRouter::rawValueFor(const P6PatchState& patch, std::uint16_t nrpn, int fallback)
{
    for (const auto& raw : patch.rawParameters)
        if (raw.id == nrpn)
            return static_cast<int>(raw.raw);

    if (const auto* serialized = findSerializedByNrpn(nrpn))
        return static_cast<int>(patch.rawPayload[serialized->sysexOffset]);

    return fallback;
}

std::pair<int, int> P6MidiRouter::rangeFor(std::uint16_t nrpn)
{
    if (nrpn == 1088)
        return {0, 1};

    SequenceNrpnAddress address{};
    if (sequenceAddressForNrpn(nrpn, address))
        return address.velocity ? std::pair<int, int>{0, 127}
                                : std::pair<int, int>{12, 108};

    if (nrpn >= 236 && nrpn <= 255)
        return {32, 125};

    if (const auto* metadata = findProgramParameter(nrpn))
        return {static_cast<int>(metadata->minimum), static_cast<int>(metadata->maximum)};

    return {0, 16383};
}

MidiRouteResult P6MidiRouter::applyNrpn(P6PatchState& patch,
                                         P6PerformanceScheduler& scheduler,
                                         const MapperCalibration& calibration,
                                         NrpnUpdate update)
{
    if (update.parameter == 1088) {
        const bool playing = update.value != 0;
        scheduler.setSequencePlaying(playing);
        MidiRouteResult out;
        out.consumed = true;
        out.nrpnUpdate = NrpnUpdate{1088, static_cast<std::uint16_t>(playing ? 1 : 0)};
        out.sequencePlay = playing;
        return out;
    }

    if (!isSupportedProgramNrpn(update.parameter)) {
        MidiRouteResult out;
        out.consumed = true;
        return out;
    }

    applyRawNrpn(patch, update);
    scheduler.updatePatch(patch, calibration);

    MidiRouteResult out;
    out.consumed = true;
    out.patchChanged = true;
    out.nrpnUpdate = NrpnUpdate{
        update.parameter,
        static_cast<std::uint16_t>(std::max(0, rawValueFor(patch, update.parameter, update.value)))
    };
    return out;
}

void P6MidiRouter::setInputChannel(int channel, P6PerformanceScheduler& scheduler) noexcept
{
    channel=std::clamp(channel,0,16);
    if(channel==inputChannel_)return;
    scheduler.midiAllSoundOff();scheduler.resetMidiControllers();reset();clearNoteOwnership();inputChannel_=channel;
}

std::array<MidiMonitorEvent,2> P6MidiRouter::recentMidiEvents() const noexcept
{
    std::array<MidiMonitorEvent,2> result{};
    const auto revision=monitorRevision_.load(std::memory_order_acquire);
    for(std::size_t i=0;i<result.size();++i) {
        if(revision<=i)break;
        const auto bits=monitorEvents_[(revision-1-static_cast<unsigned>(i))%monitorEvents_.size()].load(std::memory_order_relaxed);
        auto& e=result[i];e.valid=(bits&0x80000000u)!=0;e.pitchBend=(bits&0x40000000u)!=0;
        e.filtered=(bits&0x00800000u)!=0;e.channel=static_cast<int>((bits>>24)&31u);
        e.controller=static_cast<int>((bits>>16)&127u);e.value=static_cast<int>(bits&16383u);
    }
    return result;
}

MidiRouteResult P6MidiRouter::process(std::span<const std::uint8_t> message,
                                      P6PatchState& patch,
                                      P6PerformanceScheduler& scheduler,
                                      const MapperCalibration& calibration)
{
    MidiRouteResult out;
    if (message.empty())
        return out;

    const auto status = message[0];
    switch (status) {
    case 0xF8: scheduler.midiTimingClock(); out.consumed = true; return out;
    case 0xFA: scheduler.midiClockStart(); out.consumed = true; return out;
    case 0xFB: scheduler.midiClockContinue(); out.consumed = true; return out;
    case 0xFC: scheduler.midiClockStop(); out.consumed = true; return out;
    default: break;
    }

    if (status == 0xF0) {
        out.consumed = true;
        out.sysexDeferred = true;
        return out;
    }

    if (status >= 0xF0 || message.size() < 2)
        return out;

    const auto type = static_cast<std::uint8_t>(status & 0xF0u);
    const auto d1 = static_cast<std::uint8_t>(message[1] & 0x7Fu);
    const auto d2 = message.size() >= 3 ? static_cast<std::uint8_t>(message[2] & 0x7Fu) : 0;
    const int channel=static_cast<int>((status & 0x0Fu)+1);
    const bool filtered=inputChannel_!=0 && channel!=inputChannel_;
    if((type==0xB0 || type==0xE0) && message.size()>=3) {
        const auto value=type==0xE0?static_cast<unsigned>(d1 | (d2<<7)):static_cast<unsigned>(d2);
        const auto bits=0x80000000u | (type==0xE0?0x40000000u:0u) | (static_cast<unsigned>(channel)<<24)
            | (filtered?0x00800000u:0u) | (type==0xB0?static_cast<unsigned>(d1)<<16:0u) | value;
        const auto revision=monitorRevision_.load(std::memory_order_relaxed);
        monitorEvents_[revision%monitorEvents_.size()].store(bits,std::memory_order_relaxed);
        monitorRevision_.store(revision+1,std::memory_order_release);
    }
    if(filtered)return out;

    switch (type) {
    case 0x80:
    case 0x90: {
        const auto bit=static_cast<std::uint16_t>(1u << (channel-1));
        auto& owners=noteChannels_[d1];
        if(type==0x80 || d2==0) {
            if((owners & bit)!=0) {
                owners=static_cast<std::uint16_t>(owners & static_cast<std::uint16_t>(~bit));
                if(owners==0)scheduler.noteOff(d1);
            }
        } else {
            const auto prior=owners;owners=static_cast<std::uint16_t>(owners | bit);
            // Same-note inputs on different channels coalesce in Omni. Repeated
            // note-ons from one channel retain the existing retrigger behavior.
            if(prior==0 || (prior & bit)!=0)scheduler.noteOn(d1,static_cast<double>(d2)/127.0);
        }
        out.consumed=true;
        return out;
    }
    case 0xB0: {
        auto& decoder=nrpn_[status & 0x0Fu];
        if (d1 == 96 || d1 == 97) {
            const auto selected = decoder.selectedParameter();
            if (!selected) {
                out.consumed = true;
                return out;
            }
            const auto [minimum, maximum] = rangeFor(*selected);
            const int current = *selected == 1088
                ? (scheduler.sequencePlaying() ? 1 : 0)
                : std::clamp(rawValueFor(patch, *selected, minimum), minimum, maximum);
            const int next = std::clamp(current + (d1 == 96 ? 1 : -1), minimum, maximum);
            return applyNrpn(patch, scheduler, calibration,
                             { *selected, static_cast<std::uint16_t>(next) });
        }

        if (const auto update = decoder.processCC(d1, d2))
            return applyNrpn(patch, scheduler, calibration, *update);

        if (d1 == 1)
            scheduler.setModWheel(static_cast<double>(d2) / 127.0);
        else if (d1 == 7)
            scheduler.setMidiVolume(static_cast<double>(d2) / 127.0);
        else if (d1 == 64)
            scheduler.setSustainPedal(d2 >= 64);
        else if (d1 == 120) {
            clearNoteOwnership();scheduler.midiAllSoundOff();
        }
        else if (d1 == 121) {
            scheduler.resetMidiControllers();
            pitchBendRaw14_.store(8192, std::memory_order_relaxed);
            pitchBendRevision_.fetch_add(1, std::memory_order_relaxed);
        }
        else if (d1 == 123) {
            clearNoteOwnership();scheduler.midiAllNotesOff();
        }

        out.consumed = d1 == 1 || d1 == 7 || d1 == 64 || d1 == 120
                    || d1 == 121 || d1 == 123
                    || d1 == 99 || d1 == 98 || d1 == 6 || d1 == 38
                    || d1 == 100 || d1 == 101;
        return out;
    }
    case 0xD0:
        scheduler.setChannelPressure(static_cast<double>(d1) / 127.0);
        out.consumed = true;
        return out;
    case 0xE0: {
        const int raw14 = static_cast<int>(d1) | (static_cast<int>(d2) << 7);
        const double bipolar = raw14 >= 8192
            ? static_cast<double>(raw14 - 8192) / 8191.0
            : static_cast<double>(raw14 - 8192) / 8192.0;
        pitchBendRaw14_.store(raw14, std::memory_order_relaxed);
        pitchBendRevision_.fetch_add(1, std::memory_order_relaxed);
        scheduler.setPitchBend(bipolar);
        out.consumed = true;
        return out;
    }
    default:
        return out;
    }
}

} // namespace p6
