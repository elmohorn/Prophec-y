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

target_sources(P6CorePlugin PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
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
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
