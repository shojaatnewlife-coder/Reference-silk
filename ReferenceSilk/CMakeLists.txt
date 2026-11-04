cmake_minimum_required(VERSION 3.22)

project(ReferenceSilk VERSION 2.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(REF_BUILD_PLUGIN "Build the VST3 plugin (needs JUCE)" ON)
option(REF_BUILD_TESTS  "Build the DSP tests (no JUCE needed)" OFF)
option(REF_BUILD_TOOLS  "Build developer tools" OFF)
set(REF_JUCE_PATH "" CACHE PATH "Optional: path to a local JUCE checkout (otherwise it is downloaded)")

if(MSVC)
    add_compile_options(/MP /utf-8)
endif()

# Link the Microsoft C++ runtime into the plugin itself, so it never depends on the host's VC runtime.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(REF_BUILD_TESTS)
    add_executable(silk_tests tests/test_main.cpp)
    target_include_directories(silk_tests PRIVATE Source/core)
    add_executable(invite_tests tests/test_invite.cpp Source/Invite/RefInvite.cpp)
endif()

if(REF_BUILD_PLUGIN)
    if(REF_JUCE_PATH)
        add_subdirectory(${REF_JUCE_PATH} JUCE)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        8.0.15
            GIT_SHALLOW    TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    if(APPLE)
        set(REF_FORMATS VST3 AU)
    else()
        set(REF_FORMATS VST3)
    endif()

    juce_add_plugin(ReferenceSilk
        COMPANY_NAME             "Reference"
        COMPANY_WEBSITE          ""
        BUNDLE_ID                com.reference.silk
        PLUGIN_MANUFACTURER_CODE Rfnc
        PLUGIN_CODE              RfSk
        PRODUCT_NAME             "Reference Silk"
        DESCRIPTION              "Transformer silk: RED (highs) and BLUE (lows)"
        FORMATS                  ${REF_FORMATS}
        AU_MAIN_TYPE             kAudioUnitType_Effect
        HARDENED_RUNTIME_ENABLED FALSE
        IS_SYNTH                 FALSE
        NEEDS_MIDI_INPUT         FALSE
        NEEDS_MIDI_OUTPUT        FALSE
        IS_MIDI_EFFECT           FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES          Fx Distortion
        COPY_PLUGIN_AFTER_BUILD  FALSE)

    juce_add_binary_data(SilkFonts SOURCES
        Resources/Inter-ExtraLight.otf
        Resources/Inter-Light.otf)

    set(SILK_PLUGIN_SOURCES
        Source/plugin/PluginProcessor.cpp
        Source/plugin/PluginEditor.cpp
        Source/Invite/RefInvite.cpp
        Source/Invite/InvitePanel.cpp)

    target_sources(ReferenceSilk PRIVATE ${SILK_PLUGIN_SOURCES})
    target_include_directories(ReferenceSilk PRIVATE Source/core)
    target_compile_definitions(ReferenceSilk PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(ReferenceSilk
        PRIVATE
            SilkFonts
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    # developer tool: renders the editor in every theme
    if(REF_BUILD_TOOLS)
        juce_add_console_app(silk_snapshot PRODUCT_NAME "silk_snapshot")
        target_sources(silk_snapshot PRIVATE tools/Snapshot.cpp ${SILK_PLUGIN_SOURCES})
        target_include_directories(silk_snapshot PRIVATE Source/core)
        target_compile_definitions(silk_snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries(silk_snapshot PRIVATE SilkFonts juce::juce_audio_utils juce::juce_recommended_config_flags)
    endif()
endif()
