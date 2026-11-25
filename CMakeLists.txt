cmake_minimum_required(VERSION 3.22)

project(MagicEQ VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build a universal binary (Apple Silicon + Intel) on macOS.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# JUCE: downloaded automatically the first time you configure the project.
# To use a copy you already have, pass -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# On macOS, copy the built plugins into ~/Library/Audio/Plug-Ins so your DAW finds them.
# On Windows this needs an administrator terminal, so it is off by default there.
if(APPLE)
    set(_copy_default ON)
else()
    set(_copy_default OFF)
endif()
option(MAGICEQ_COPY_AFTER_BUILD "Install plugins into the system plugin folders after building" ${_copy_default})

set(MAGICEQ_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND MAGICEQ_FORMATS AU)
endif()

juce_add_plugin(MagicEQ
    COMPANY_NAME                "RainWorks"
    COMPANY_WEBSITE             "https://github.com/dfato/magiceq"
    BUNDLE_ID                   com.rainworks.magiceq
    PLUGIN_MANUFACTURER_CODE    Rnwk
    PLUGIN_CODE                 MgEq
    PRODUCT_NAME                "MagicEQ"
    FORMATS                     ${MAGICEQ_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx EQ
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     ${MAGICEQ_COPY_AFTER_BUILD})

juce_generate_juce_header(MagicEQ)

set(MAGICEQ_CORE_SOURCES
    Source/core/Biquad.cpp
    Source/core/StyleLibrary.cpp
    Source/core/SpectrumAccumulator.cpp
    Source/core/MatchFitter.cpp)

target_sources(MagicEQ PRIVATE
    ${MAGICEQ_CORE_SOURCES}
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/RainLookAndFeel.cpp
    Source/ui/EqGraph.cpp
    Source/ui/BandPanel.cpp)

target_include_directories(MagicEQ PRIVATE Source)

target_compile_definitions(MagicEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(MagicEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Automated tests for the DSP and Detect logic (no DAW needed).
# Run with:  ctest --test-dir build -C Release --output-on-failure
# ---------------------------------------------------------------------------
option(MAGICEQ_BUILD_TESTS "Build the MagicEQ test program" ON)
if(MAGICEQ_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(MagicEQTests PRODUCT_NAME "MagicEQTests")
    target_sources(MagicEQTests PRIVATE ${MAGICEQ_CORE_SOURCES} Tests/TestMain.cpp)
    target_include_directories(MagicEQTests PRIVATE Source)
    target_compile_definitions(MagicEQTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(MagicEQTests
        PRIVATE juce::juce_core juce::juce_audio_basics juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME MagicEQTests COMMAND MagicEQTests)
endif()

# ---------------------------------------------------------------------------
# Developer tool that renders a screenshot of the UI after a Detect run (Linux/CI only).
# ---------------------------------------------------------------------------
option(MAGICEQ_BUILD_SNAPSHOT "Build the screenshot tool" OFF)
if(MAGICEQ_BUILD_SNAPSHOT)
    juce_add_console_app(MagicEQSnapshot PRODUCT_NAME "MagicEQSnapshot")
    target_sources(MagicEQSnapshot PRIVATE
        ${MAGICEQ_CORE_SOURCES}
        Source/PluginProcessor.cpp Source/PluginEditor.cpp
        Source/ui/RainLookAndFeel.cpp Source/ui/EqGraph.cpp Source/ui/BandPanel.cpp
        Tools/Snapshot.cpp)
    target_include_directories(MagicEQSnapshot PRIVATE Source)
    juce_generate_juce_header(MagicEQSnapshot)
    target_compile_definitions(MagicEQSnapshot PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="MagicEQ" JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(MagicEQSnapshot PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
endif()
