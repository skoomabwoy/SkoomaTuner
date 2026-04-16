cmake_minimum_required(VERSION 3.22)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "")
project(SkoomaTuner VERSION 1.0.2)

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

# Build FFTW3 (single-precision) from source as a static library
FetchContent_Declare(
    fftw3
    URL      https://www.fftw.org/fftw-3.3.10.tar.gz
    URL_HASH MD5=8ccbf6a5ea78a16dbc3e1306e234cc5c
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_FLOAT ON CACHE BOOL "" FORCE)
set(ENABLE_THREADS OFF CACHE BOOL "" FORCE)
set(ENABLE_OPENMP OFF CACHE BOOL "" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
FetchContent_MakeAvailable(fftw3)

# DSP sources (extracted from StompTuner)
set(DSP_SOURCES
    Source/dsp/pitch_tracker.cpp
    Source/dsp/low_high_cut.cc
)

juce_add_plugin(SkoomaTuner
    COMPANY_NAME       "skoomaDSP"
    PLUGIN_MANUFACTURER_CODE Skom
    PLUGIN_CODE        SkTn
    FORMATS            VST3
    PRODUCT_NAME       "skoomaTuner"
    IS_SYNTH           FALSE
    NEEDS_MIDI_INPUT   FALSE
    NEEDS_MIDI_OUTPUT  FALSE
    IS_MIDI_EFFECT     FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

# Embedded font + SVG icon binary data
juce_add_binary_data(FontData
    SOURCES
        Source/fonts/JetBrainsMono-Bold-subset.ttf
        Source/icons/theme.svg
        Source/icons/meter.svg
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

find_package(Threads REQUIRED)

target_link_libraries(SkoomaTuner
    PRIVATE
        FontData
        juce::juce_audio_utils
        fftw3f
        Threads::Threads
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

target_include_directories(SkoomaTuner PRIVATE ${fftw3_SOURCE_DIR}/api)
