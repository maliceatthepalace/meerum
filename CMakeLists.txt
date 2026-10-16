cmake_minimum_required(VERSION 3.22)

project(Meerum VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    # Universal binary so the plugin loads in both Apple Silicon and Rosetta hosts.
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

add_subdirectory(external/JUCE)

set(MEERUM_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND MEERUM_FORMATS AU)
endif()
if(UNIX AND NOT APPLE)
    list(APPEND MEERUM_FORMATS LV2)
endif()

option(MEERUM_COPY_AFTER_BUILD "Install the plugin into the user's plugin folders after building" OFF)

juce_add_plugin(Meerum
    COMPANY_NAME "maliceatthepalace"
    BUNDLE_ID "com.maliceatthepalace.meerum"
    PLUGIN_MANUFACTURER_CODE Mlcp
    PLUGIN_CODE Meer
    FORMATS ${MEERUM_FORMATS}
    PRODUCT_NAME "Meerum"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    LV2URI "https://github.com/maliceatthepalace/meerum"
    COPY_PLUGIN_AFTER_BUILD ${MEERUM_COPY_AFTER_BUILD})

file(GLOB_RECURSE MEERUM_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h)
target_sources(Meerum PRIVATE ${MEERUM_SOURCES})
target_include_directories(Meerum PRIVATE Source)

target_compile_definitions(Meerum
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Meerum
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline render/smoke test (renders presets to WAV, checks state and CPU).
option(MEERUM_BUILD_TESTS "Build the offline render test" ON)
if(MEERUM_BUILD_TESTS)
    juce_add_console_app(MeerumRenderTest PRODUCT_NAME "MeerumRenderTest")
    target_sources(MeerumRenderTest PRIVATE tests/RenderTest.cpp ${MEERUM_SOURCES})
    target_include_directories(MeerumRenderTest PRIVATE Source)
    target_compile_definitions(MeerumRenderTest PRIVATE
        JucePlugin_Name="Meerum"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(MeerumRenderTest
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_recommended_config_flags)
endif()
