cmake_minimum_required(VERSION 3.22)
project(SlayEngine VERSION 1.0.0)

add_subdirectory(JUCE)

juce_add_plugin(SlayEngine
    COMPANY_NAME "Skairacks"
    PLUGIN_NAME "SlayEngine"
    FORMATS VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
)

target_sources(SlayEngine PRIVATE PluginProcessor.cpp)

target_link_libraries(SlayEngine
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
