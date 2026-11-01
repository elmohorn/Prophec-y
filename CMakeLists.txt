cmake_minimum_required(VERSION 3.22)
project(P6Core VERSION 0.20.0 LANGUAGES C CXX)

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
    src/P6EffectMapper.cpp
    src/P6ModulationEffects.cpp
    src/P6ReverbEffects.cpp
    src/ParameterRegistry.cpp
    src/ProgramLayout.cpp
    src/ReferenceIsolation.cpp
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
add_library(p6character plugin/CharacterFx.cpp)
target_include_directories(p6character PUBLIC plugin PRIVATE third_party/DSPark)
target_link_libraries(p6character PUBLIC p6core)
set_target_properties(p6character PROPERTIES POSITION_INDEPENDENT_CODE ON)
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

add_executable(p6_v019_reference_tests tests/v019_reference_isolation_test.cpp)
target_link_libraries(p6_v019_reference_tests PRIVATE p6core)
if(MSVC)
  target_compile_options(p6_v019_reference_tests PRIVATE /UNDEBUG)
else()
  target_compile_options(p6_v019_reference_tests PRIVATE -UNDEBUG)
endif()
add_test(NAME p6_v019_reference_tests COMMAND p6_v019_reference_tests)

add_executable(p6_character_tests tests/character_fx_test.cpp)
target_link_libraries(p6_character_tests PRIVATE p6character)
add_test(NAME p6_character_tests COMMAND p6_character_tests)

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
add_executable(p6_render_reference tools/render_reference.cpp)
target_link_libraries(p6_render_reference PRIVATE p6core)
add_test(NAME p6_render_reference_help COMMAND p6_render_reference --help)

add_executable(p6_generate_p0a_capture_pack tools/generate_p0a_capture_pack.cpp)
target_link_libraries(p6_generate_p0a_capture_pack PRIVATE p6core)
add_test(NAME p6_generate_p0a_capture_pack_self_test
         COMMAND p6_generate_p0a_capture_pack --self-test)
add_executable(p6_sync_probe tools/sync_probe.cpp)
target_link_libraries(p6_sync_probe PRIVATE p6core)
add_executable(p6_polymod_probe tools/polymod_probe.cpp)
target_link_libraries(p6_polymod_probe PRIVATE p6core)
add_executable(p6_sync_microscope tools/sync_microscope.cpp)
target_link_libraries(p6_sync_microscope PRIVATE p6core)
add_executable(p6_sync_harmonic_probe tools/sync_harmonic_probe.cpp)
target_link_libraries(p6_sync_harmonic_probe PRIVATE p6core)
add_executable(p6_timing_probe tools/timing_probe.cpp)
target_link_libraries(p6_timing_probe PRIVATE p6core)
add_executable(p6_perf_benchmark tools/perf_benchmark.cpp)
target_link_libraries(p6_perf_benchmark PRIVATE p6core)
add_executable(p6_dsp_fingerprint tools/dsp_fingerprint.cpp)
target_link_libraries(p6_dsp_fingerprint PRIVATE p6core)

# Parameter presentation remains independent of JUCE and audio-thread state.
add_library(p6presentation plugin/ui/ParameterPresentation.cpp)
target_include_directories(p6presentation PUBLIC plugin)
target_link_libraries(p6presentation PUBLIC p6core)
set_target_properties(p6presentation PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_executable(p6_parameter_presentation_tests tests/parameter_presentation_test.cpp)
target_link_libraries(p6_parameter_presentation_tests PRIVATE p6presentation)
add_test(NAME p6_parameter_presentation_tests COMMAND p6_parameter_presentation_tests)
add_executable(p6_state_compatibility_tests tests/state_compatibility_test.cpp plugin/PluginStateCodec.cpp)
target_include_directories(p6_state_compatibility_tests PRIVATE plugin)
target_link_libraries(p6_state_compatibility_tests PRIVATE p6core)
target_compile_definitions(p6_state_compatibility_tests PRIVATE P6_STATE_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/state_versions")
add_test(NAME p6_state_compatibility_tests COMMAND p6_state_compatibility_tests)

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

add_executable(p6_parameter_domain_audit tests/parameter_domain_audit_test.cpp)
target_link_libraries(p6_parameter_domain_audit PRIVATE p6core)
add_test(NAME p6_parameter_domain_audit COMMAND p6_parameter_domain_audit)
