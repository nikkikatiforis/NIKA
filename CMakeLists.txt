cmake_minimum_required(VERSION 3.22)
project(NIKA VERSION 1.0.0)

set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE")
add_subdirectory(${JUCE_PATH} JUCE)

juce_add_plugin(NIKA
    COMPANY_NAME "NIKA"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Nika
    PLUGIN_CODE NKsy
    FORMATS VST3 Standalone
    PRODUCT_NAME "NIKA"
)

target_sources(NIKA PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Oscillator.cpp
    Source/LadderFilter.cpp
    Source/ADSR.cpp
    Source/Compressor.cpp
    Source/Saturator.cpp
    Source/Limiter.cpp
    Source/KeyswitchEngine.cpp
    Source/FXEngine.cpp
)

target_compile_definitions(NIKA PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

juce_generate_juce_header(NIKA)

target_link_libraries(NIKA PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_audio_processors
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

add_custom_command(TARGET NIKA_VST3 POST_BUILD
    COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:NIKA_VST3>"
    COMMENT "Ad-hoc signing NIKA.vst3"
)
