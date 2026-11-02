option(P6_BUILD_STANDALONE "Also build the JUCE Standalone target" OFF)
set(P6_PLUGIN_FORMATS VST3)
if(P6_BUILD_STANDALONE)
    list(APPEND P6_PLUGIN_FORMATS Standalone)
endif()

juce_add_plugin(P6CorePlugin
    COMPANY_NAME "P6 Core Project"
    BUNDLE_ID "com.elmohorn.p6core"
    PLUGIN_MANUFACTURER_CODE P6Co
    PLUGIN_CODE P6cr
    FORMATS ${P6_PLUGIN_FORMATS}
    PRODUCT_NAME "P6 Core"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

juce_add_binary_data(P6PanelAssets SOURCES assets/tube_glass.png assets/tube_chamber.jpg)
set_target_properties(P6PanelAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(P6CorePlugin PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    OutputStage.h
    PresetBankCodec.cpp
    PresetBankCodec.h
    PresetLibrary.cpp
    PresetLibrary.h
    ui/preset/PresetBrowser.cpp
    ui/preset/PresetBrowser.h
    PluginEditor.cpp
    PluginEditor.h
    ui/controls/ProphecyParameterControl.cpp
    ui/controls/ProphecyParameterControl.h
    ui/fx/CharacterDeviceView.cpp
    ui/fx/CharacterDeviceView.h
    ui/fx/TubeChamber.cpp
    ui/fx/TubeChamber.h
    ui/display/OutputMeter.cpp
    ui/display/OutputMeter.h
    ui/display/ContextDisplay.cpp
    ui/display/ContextDisplay.h
    PluginStateCodec.cpp
    PluginStateCodec.h)

target_compile_definitions(P6CorePlugin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_JACK=0
        JUCE_PLUGINHOST_LADSPA=0)

target_link_libraries(P6CorePlugin
    PRIVATE
        p6core
        p6character
        p6presentation
        P6PanelAssets
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Keep the adopted MIT notice with binary-only VST3 artifacts as well as source.
get_target_property(P6_VST3_BUNDLE P6CorePlugin_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
add_custom_command(TARGET P6CorePlugin_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${P6_VST3_BUNDLE}/Contents/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/third_party/DSPark/LICENSE"
        "${P6_VST3_BUNDLE}/Contents/Resources/DSPark-LICENSE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/third_party/DaisySP/LICENSE"
        "${P6_VST3_BUNDLE}/Contents/Resources/DaisySP-LICENSE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        "${P6_VST3_BUNDLE}/Contents/Resources/THIRD_PARTY_NOTICES.md"
    VERBATIM)

option(P6_BUILD_UI_SMOKE "Build the native editor/state smoke executable" OFF)
if(P6_BUILD_UI_SMOKE)
    add_executable(P6RestoreProbe ../tests/restore_interleaving_probe.cpp)
    set_target_properties(P6RestoreProbe PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/P6RestoreProbe_artefacts/$<CONFIG>")
    target_include_directories(P6RestoreProbe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(P6RestoreProbe PRIVATE P6CorePlugin p6character juce::juce_audio_processors)
    add_executable(P6UiSmoke ../tests/plugin_ui_smoke.cpp)
    set_target_properties(P6UiSmoke PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/P6UiSmoke_artefacts/$<CONFIG>")
    target_include_directories(P6UiSmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(P6UiSmoke PRIVATE P6CorePlugin p6character juce::juce_audio_processors)
endif()

