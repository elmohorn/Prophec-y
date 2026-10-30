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

    switch (type) {
    case 0x80:
        scheduler.noteOff(d1);
        out.consumed = true;
        return out;
    case 0x90:
        if (d2 == 0) scheduler.noteOff(d1);
        else scheduler.noteOn(d1, static_cast<double>(d2) / 127.0);
        out.consumed = true;
        return out;
    case 0xB0: {
        if (d1 == 96 || d1 == 97) {
            const auto selected = nrpn_.selectedParameter();
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

        if (const auto update = nrpn_.processCC(d1, d2))
            return applyNrpn(patch, scheduler, calibration, *update);

        if (d1 == 1) {
            scheduler.setModWheel(static_cast<double>(d2) / 127.0);
        } else if (d1 == 7) {
            // Received Controller Messages: MIDI Volume is combined with the
            // instrument's other volume stages. Keep it as runtime gain rather
            // than rewriting the stored Voice Volume parameter.
            scheduler.setMidiVolume(static_cast<double>(d2) / 127.0);
        } else if (d1 == 64) {
            scheduler.setSustainPedal(d2 >= 64);
        } else if (d1 == 121) {
            // Prophet-6: reset controllers to zero, except MIDI Volume which
            // returns to maximum.
            scheduler.setModWheel(0.0);
            scheduler.setSustainPedal(false);
            scheduler.setMidiVolume(1.0);
        } else if (d1 == 120 || d1 == 123) {
            scheduler.allNotesOff();
        }

        out.consumed = d1 == 1 || d1 == 7 || d1 == 64
                    || d1 == 120 || d1 == 121 || d1 == 123
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
        scheduler.setPitchBend(bipolar);
        out.consumed = true;
        return out;
    }
    default:
        return out;
    }
}

} // namespace p6
