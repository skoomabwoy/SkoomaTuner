cmake_minimum_required(VERSION 3.22)
project(SkoomaTuner VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# Find FFTW3 (single-precision)
find_library(FFTW3F_LIB fftw3f REQUIRED)
find_path(FFTW3_INCLUDE_DIR fftw3.h REQUIRED)

# DSP sources (extracted from StompTuner)
set(DSP_SOURCES
    Source/dsp/pitch_tracker.cpp
    Source/dsp/tuner.cc
    Source/dsp/low_high_cut.cc
    Source/dsp/resampler.cc
    Source/dsp/resampler-table.cc
)

juce_add_plugin(SkoomaTuner
    COMPANY_NAME       "SkoomaDSP"
    PLUGIN_MANUFACTURER_CODE Skom
    PLUGIN_CODE        SkTn
    FORMATS            VST3
    PRODUCT_NAME       "SkoomaTuner"
    IS_SYNTH           FALSE
    NEEDS_MIDI_INPUT   FALSE
    NEEDS_MIDI_OUTPUT  FALSE
    IS_MIDI_EFFECT     FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

# Embedded font binary data
juce_add_binary_data(FontData
    SOURCES
        Source/fonts/JetBrainsMono-Bold-subset.ttf
)

target_sources(SkoomaTuner
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        ${DSP_SOURCES}
)

target_include_directories(SkoomaTuner
    PRIVATE
        Source/dsp
)

target_compile_definitions(SkoomaTuner
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SkoomaTuner
    PRIVATE
        FontData
        juce::juce_audio_utils
        ${FFTW3F_LIB}
        pthread
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

target_include_directories(SkoomaTuner PRIVATE ${FFTW3_INCLUDE_DIR})
