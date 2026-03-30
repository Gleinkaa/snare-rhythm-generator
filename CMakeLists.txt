cmake_minimum_required(VERSION 3.22)
project(SnareRhythmGenerator VERSION 1.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# Plugin target
juce_add_plugin(SnareRhythmGenerator
    COMPANY_NAME           "Gleinkaa"
    PLUGIN_MANUFACTURER_CODE Snrg
    PLUGIN_CODE             SRhG
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "SnareRhythmGen"
    BUNDLE_ID               "com.gleinkaa.snare-rhythm-gen"
    IS_MIDI_EFFECT          FALSE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       TRUE
    IS_SYNTH                TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
)

juce_generate_juce_header(SnareRhythmGenerator)

target_sources(SnareRhythmGenerator
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_compile_definitions(SnareRhythmGenerator
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(SnareRhythmGenerator
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
