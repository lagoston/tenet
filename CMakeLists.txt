cmake_minimum_required(VERSION 3.22)

project(LagostomGlue VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(PLUGIN_FORMATS VST3 AU)
else()
    set(PLUGIN_FORMATS VST3)
endif()

juce_add_plugin(LagostomGlue
    COMPANY_NAME "Lagostom Productions"
    PLUGIN_MANUFACTURER_CODE Lgst
    PLUGIN_CODE Lglu
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Lagostom Glue"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics Distortion)

target_sources(LagostomGlue PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h)

target_compile_definitions(LagostomGlue PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(LagostomGlue
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
