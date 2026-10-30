cmake_minimum_required(VERSION 3.22)
project(P6Core VERSION 0.18.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(p6core
    src/SysexCodec.cpp
    src/SysexStream.cpp
    src/P6Sequence.cpp
    src/P6Clock.cpp
    src/P6Arpeggiator.cpp
    src/P6SequencerPlayer.cpp
    src/P6PerformanceScheduler.cpp
    src/P6PerformanceMidiMode.cpp
    src/P6MidiRouter.cpp
    src/PolyBLEPOscillator.cpp
    src/ADSREnvelope.cpp
    src/AnalogSlop.cpp
    src/P6LFO.cpp
    src/P6Effects.cpp
    src/ParameterRegistry.cpp
    src/ProgramLayout.cpp
    src/ParameterMapper.cpp
    src/CandidateFilters.cpp
    src/P6Voice.cpp
    src/NrpnCodec.cpp
    src/P6Engine.cpp
    src/P6EngineMidiMode.cpp
)

target_include_directories(p6core PUBLIC include)
target_compile_features(p6core PUBLIC cxx_std_20)
set_target_properties(p6core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
  target_compile_options(p6core PRIVATE /W4 /permissive-)
else()
  target_compile_options(p6core PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_executable(p6core_tests tests/test_main.cpp)
target_link_libraries(p6core_tests PRIVATE p6core)
if(MSVC)
  target_compile_options(p6core_tests PRIVATE /UNDEBUG)
else()
  target_compile_options(p6core_tests PRIVATE -UNDEBUG)
endif()
add_test(NAME p6core_tests COMMAND p6core_tests)

add_executable(p6_plugin_state_tests
    tests/plugin_state_test.cpp
    plugin/PluginStateCodec.cpp)
target_include_directories(p6_plugin_state_tests PRIVATE plugin)
target_link_libraries(p6_plugin_state_tests PRIVATE p6core)
if(MSVC)
  target_compile_options(p6_plugin_state_tests PRIVATE /UNDEBUG)
else()
  target_compile_options(p6_plugin_state_tests PRIVATE -UNDEBUG)
endif()
add_test(NAME p6_plugin_state_tests COMMAND p6_plugin_state_tests)

find_package(Threads REQUIRED)
add_executable(p6_control_contract_tests tests/control_contract_test.cpp)
target_link_libraries(p6_control_contract_tests PRIVATE p6core Threads::Threads)
add_test(NAME p6_control_contract_tests COMMAND p6_control_contract_tests)

# Test-only global new/delete instrumentation proves that the prepared core
# realtime path (MIDI routing -> scheduler -> engine) performs no ordinary heap
# allocations during sustained processing. All setup/reserve work happens first.
add_executable(p6_realtime_contract_tests tests/realtime_contract_test.cpp)
target_link_libraries(p6_realtime_contract_tests PRIVATE p6core Threads::Threads)
add_test(NAME p6_realtime_contract_tests COMMAND p6_realtime_contract_tests)

add_executable(p6_v018_fx_tests tests/v018_fx_test.cpp)
target_link_libraries(p6_v018_fx_tests PRIVATE p6core)
if(MSVC)
  target_compile_options(p6_v018_fx_tests PRIVATE /UNDEBUG)
else()
  target_compile_options(p6_v018_fx_tests PRIVATE -UNDEBUG)
endif()
add_test(NAME p6_v018_fx_tests COMMAND p6_v018_fx_tests)

add_executable(p6_render tools/render_patch.cpp)
target_link_libraries(p6_render PRIVATE p6core)
add_executable(p6_inspect tools/inspect_sysex.cpp)
target_link_libraries(p6_inspect PRIVATE p6core)
add_executable(p6_sequence_dump tools/sequence_dump.cpp)
target_link_libraries(p6_sequence_dump PRIVATE p6core)
add_executable(p6_render_events tools/render_events.cpp)
target_link_libraries(p6_render_events PRIVATE p6core)
add_executable(p6_render_cal tools/render_calibration.cpp)
target_link_libraries(p6_render_cal PRIVATE p6core)
add_executable(p6_render_events_cal tools/render_events_calibration.cpp)
target_link_libraries(p6_render_events_cal PRIVATE p6core)
add_executable(p6_sync_probe tools/sync_probe.cpp)
target_link_libraries(p6_sync_probe PRIVATE p6core)
add_executable(p6_polymod_probe tools/polymod_probe.cpp)
target_link_libraries(p6_polymod_probe PRIVATE p6core)
add_executable(p6_sync_microscope tools/sync_microscope.cpp)
target_link_libraries(p6_sync_microscope PRIVATE p6core)
add_executable(p6_timing_probe tools/timing_probe.cpp)
target_link_libraries(p6_timing_probe PRIVATE p6core)

option(P6_BUILD_PLUGIN "Build the JUCE VST3/Standalone wrapper" OFF)
option(P6_FETCH_JUCE "Fetch the pinned JUCE release if it is not otherwise available" ON)
set(P6_JUCE_PATH "" CACHE PATH "Path to a JUCE source checkout (optional when JUCE is installed)")
set(P6_JUCE_TAG "9.0.4" CACHE STRING "Pinned JUCE release used by FetchContent")

if(P6_BUILD_PLUGIN)
  if(P6_JUCE_PATH)
    add_subdirectory("${P6_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
  else()
    find_package(JUCE CONFIG QUIET)
    if(NOT TARGET juce::juce_audio_processors)
      if(P6_FETCH_JUCE)
        include(FetchContent)
        FetchContent_Declare(JUCE
          GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
          GIT_TAG        ${P6_JUCE_TAG}
          GIT_SHALLOW    TRUE)
        FetchContent_MakeAvailable(JUCE)
      else()
        message(FATAL_ERROR
          "JUCE was not found. Set P6_JUCE_PATH, install JUCE as a CMake package, "
          "or configure with -DP6_FETCH_JUCE=ON.")
      endif()
    endif()
  endif()
  add_subdirectory(plugin)
endif()