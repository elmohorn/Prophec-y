#pragma once
#include "p6/ParameterMapper.h"
#include "p6/ParameterRegistry.h"
#include <optional>
#include <string>
#include <string_view>

namespace p6plugin::ui {
struct ParameterContext {
    p6::MapperCalibration calibration{};
    bool lfoSynced{};
};
enum class ValueUnit { Raw, Percent, Seconds, Semitones, Cents, Hertz, Morph };
struct PresentedValue { double value{}; ValueUnit unit{ValueUnit::Raw}; };

// UI-side functions, independent of JUCE. Parameter IDs/domains are borrowed
// from the canonical registry; semantic conversions use the active model.
PresentedValue parameterValue(int nrpn, double raw, const ParameterContext&);
std::string formatParameterValue(int nrpn, double raw, const ParameterContext&);
std::optional<double> parseParameterValue(int nrpn, std::string_view text,
                                         const ParameterContext&, bool rawInput=false);
std::string parameterHelp(int nrpn);
} // namespace p6plugin::ui
