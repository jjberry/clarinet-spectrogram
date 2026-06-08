cmake_minimum_required(VERSION 3.22)
project(ClariSynth VERSION 0.1.0)

add_subdirectory(JUCE)

juce_add_plugin(ClariSynth
    COMPANY_NAME           "JeffBerry"
    PLUGIN_MANUFACTURER_CODE Jbry
    PLUGIN_CODE            Clrs
    FORMATS                VST3 AU Standalone
    PRODUCT_NAME           "ClariSynth"
    IS_SYNTH               FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
)

target_sources(ClariSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/HPSPitchDetector.cpp
    Source/DSP/BandpassProcessor.cpp
    ../Sources/DSP/SpectralProcessor.c
)

target_include_directories(ClariSynth PRIVATE
    Source
    ../Sources/DSP
)

target_compile_definitions(ClariSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ClariSynth PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    "-framework Accelerate"
)

# ---- Unit tests ----
enable_testing()

add_executable(ClariSynthTests Tests/ChannelRoutingTests.cpp)
target_compile_features(ClariSynthTests PRIVATE cxx_std_17)
target_include_directories(ClariSynthTests PRIVATE Source/DSP)

add_test(NAME ChannelRoutingTests COMMAND ClariSynthTests)
