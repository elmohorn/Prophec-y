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

set(P6_PANEL_ASSETS assets/tube_glass.png assets/tube_chamber.jpg assets/fonts/michroma.ttf
    assets/presets/sequential_factory.syx assets/presets/sequential_omom.syx)
# Optional camera atlas uses native Tube frame; builds without asset retain fallback.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/character_cameras_atlas.jpg")
    list(APPEND P6_PANEL_ASSETS assets/character_cameras_atlas.jpg)
    set(P6_HAS_CHARACTER_ATLAS TRUE)
endif()

# Authored/interim RGBA camera layers are independently replaceable. A
# complete four-layer set is required for each module; incomplete installs
# always fall back to the embedded approved JPEG or procedural rendering.
set(P6_CHARACTER_RGBA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/character_rgba")
set(P6_SPRING_RGBA
    spring_backplate.png spring_upper_coil.png
    spring_lower_coil.png spring_reflection_overlay.png)
set(P6_OPTICAL_RGBA
    optical_backplate.png optical_lamp_off.png
    optical_lamp_on.png optical_ldr_overlay.png)
set(P6_SPRING_RGBA_COMPLETE TRUE)
set(P6_OPTICAL_RGBA_COMPLETE TRUE)
foreach(asset IN LISTS P6_SPRING_RGBA)
    if(NOT EXISTS "${P6_CHARACTER_RGBA_DIR}/${asset}")
        set(P6_SPRING_RGBA_COMPLETE FALSE)
    endif()
endforeach()
foreach(asset IN LISTS P6_OPTICAL_RGBA)
    if(NOT EXISTS "${P6_CHARACTER_RGBA_DIR}/${asset}")
        set(P6_OPTICAL_RGBA_COMPLETE FALSE)
    endif()
endforeach()
if(P6_SPRING_RGBA_COMPLETE)
    foreach(asset IN LISTS P6_SPRING_RGBA)
        list(APPEND P6_PANEL_ASSETS "assets/character_rgba/${asset}")
    endforeach()
endif()
if(P6_OPTICAL_RGBA_COMPLETE)
    foreach(asset IN LISTS P6_OPTICAL_RGBA)
        list(APPEND P6_PANEL_ASSETS "assets/character_rgba/${asset}")
    endforeach()
endif()
juce_add_binary_data(P6PanelAssets SOURCES ${P6_PANEL_ASSETS})
set_target_properties(P6PanelAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(P6CorePlugin PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    RealtimeSnapshotMailbox.h
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
    ui/fx/CharacterHardwareWindow.h
    ui/fx/CharacterHardwarePrimitives.h
    ui/fx/CharacterProceduralArt.h
    ui/fx/CharacterPhotoLayers.h
    ui/fx/CharacterVisualDynamics.h
    ui/fx/TubeChamber.cpp
    ui/fx/TubeChamber.h
    ui/display/OutputMeter.cpp
    ui/display/OutputMeter.h
    ui/display/PresetNameDisplay.h
    ui/controls/SegmentedLedRing.h
    PluginStateCodec.cpp
    PluginStateCodec.h)

if(P6_HAS_CHARACTER_ATLAS)
    target_compile_definitions(P6CorePlugin PRIVATE P6_HAS_CHARACTER_ATLAS=1)
endif()
if(P6_SPRING_RGBA_COMPLETE)
    target_compile_definitions(P6CorePlugin PRIVATE P6_HAS_SPRING_RGBA=1)
endif()
if(P6_OPTICAL_RGBA_COMPLETE)
    target_compile_definitions(P6CorePlugin PRIVATE P6_HAS_OPTICAL_RGBA=1)
endif()

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
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/third_party/GenerativeMIDI-LICENSE.txt"
        "${P6_VST3_BUNDLE}/Contents/Resources/GenerativeMIDI-LICENSE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/OFL-Michroma.txt"
        "${P6_VST3_BUNDLE}/Contents/Resources/OFL-Michroma.txt"
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
    # Verify that installed RGBA artwork, not only test fixtures, is bound
    # to the shipping editor whenever a complete asset set is present.
    if(P6_SPRING_RGBA_COMPLETE)
        target_compile_definitions(P6UiSmoke PRIVATE P6_HAS_SPRING_RGBA=1)
    endif()
    if(P6_OPTICAL_RGBA_COMPLETE)
        target_compile_definitions(P6UiSmoke PRIVATE P6_HAS_OPTICAL_RGBA=1)
    endif()

    # JUCE-free deterministic test for measured telemetry -> physical motion;
    # it can run even when no GUI host or VST3 bundle is available.
    add_executable(P6CharacterVisualProbe ../tests/character_visual_dynamics_probe.cpp)
    target_include_directories(P6CharacterVisualProbe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    # Keep 'assert' active in Release screenshot CI as well as Debug.
    if(MSVC)
        target_compile_options(P6CharacterVisualProbe PRIVATE /UNDEBUG)
    else()
        target_compile_options(P6CharacterVisualProbe PRIVATE -UNDEBUG)
    endif()
    add_test(NAME p6_character_visual_dynamics COMMAND P6CharacterVisualProbe)

    find_package(Threads REQUIRED)
    add_executable(P6TelemetryMailboxProbe ../tests/character_telemetry_mailbox_probe.cpp)
    target_include_directories(P6TelemetryMailboxProbe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(P6TelemetryMailboxProbe PRIVATE Threads::Threads)
    if(MSVC)
        target_compile_options(P6TelemetryMailboxProbe PRIVATE /UNDEBUG)
    else()
        target_compile_options(P6TelemetryMailboxProbe PRIVATE -UNDEBUG)
    endif()
    add_test(NAME p6_character_telemetry_mailbox COMMAND P6TelemetryMailboxProbe)
endif()



# Explicitly opt-in native gallery. It does not modify PluginEditor or DSP.
# Configure the root with -DP6_BUILD_PLUGIN=ON -DP6_BUILD_HARDWARE_GALLERY=ON
# then build --target P6HardwareGallery.
option(P6_BUILD_HARDWARE_GALLERY "Build the standalone native UI-component review gallery" OFF)
if(P6_BUILD_HARDWARE_GALLERY)
    juce_add_gui_app(P6HardwareGallery PRODUCT_NAME "Prophec-Y Hardware Gallery")
    target_sources(P6HardwareGallery PRIVATE
        ui/hardware/HardwareGalleryMain.cpp
        ui/hardware/HardwareTheme.h
        ui/hardware/HardwareControls.h
        ui/hardware/HardwareDisplays.h
        ui/controls/ProphecyParameterControl.cpp
        ui/controls/ProphecyParameterControl.h
        ui/controls/SegmentedLedRing.h)
    target_include_directories(P6HardwareGallery PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(P6HardwareGallery PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(P6HardwareGallery PRIVATE
        p6core p6presentation
        juce::juce_audio_processors juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
