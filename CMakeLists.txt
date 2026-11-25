cmake_minimum_required(VERSION 3.22)
project(PinkSwan VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version" FORCE)
    set(CMAKE_XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Xcode deployment target" FORCE)
endif()

include(FetchContent)

# --- JUCE ---
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
)
FetchContent_MakeAvailable(JUCE)

# --- Plugin Pink Swan ---
juce_add_plugin(PinkSwan
    COMPANY_NAME "Personal"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Pers
    PLUGIN_CODE PkSw
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Pink Swan"
)

target_sources(PinkSwan PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Dsp/Resonator.cpp
)

target_compile_definitions(PinkSwan PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_compile_options(PinkSwan PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

target_link_libraries(PinkSwan PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# --- Tests unitaires DSP (binaire console dédié, sans les tests internes de JUCE) ---
enable_testing()

juce_add_console_app(PinkSwanTests
    PRODUCT_NAME "PinkSwanTests"
)

target_sources(PinkSwanTests PRIVATE
    Source/Tests/ResonatorTest.cpp
    Source/Dsp/Resonator.cpp
)

target_compile_definitions(PinkSwanTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
)

target_compile_options(PinkSwanTests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

target_link_libraries(PinkSwanTests PRIVATE
    juce::juce_dsp
    juce::juce_audio_formats
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

add_test(NAME ResonatorUnitTests COMMAND PinkSwanTests)
