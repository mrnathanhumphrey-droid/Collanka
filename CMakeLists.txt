cmake_minimum_required(VERSION 3.22)
project(Collonka VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_plugin(Collonka
    COMPANY_NAME "BillyWonkaMusic"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Bwmx
    PLUGIN_CODE Clnk
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Collonka"
    BUNDLE_ID "com.billywonkamusic.collonka"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Collonka needs audio input access for monitoring."
    DESCRIPTION "Collatz-organized Moog-shaped bass synthesizer (Collonka, fork of BW BASS)"
)

target_sources(Collonka
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        DSP/BLOscillator.cpp
        DSP/OscillatorBank.cpp
        DSP/CollatzFormantFilter.cpp
        DSP/NoiseGenerator.cpp
        DSP/MoogLadderFilter.cpp
        DSP/EnvelopeGenerator.cpp
        DSP/PitchEnvelope.cpp
        DSP/PortamentoEngine.cpp
        DSP/LFOEngine.cpp
        DSP/ModMatrix.cpp
        DSP/MixerSection.cpp
        DSP/OutputStage.cpp
        DSP/MoogTapeSaturator.cpp
        DSP/SubHarmonicExciter.cpp
        DSP/TransientWaveshaper.cpp
        DSP/SymmetricFolder.cpp
        DSP/RnBCompressor.cpp
        DSP/BassReverb.cpp
        DSP/NoteManager.cpp
        DSP/SynthEngine.cpp
        UI/BWLookAndFeel.cpp
        UI/BWKnob.cpp
        UI/HeaderBar.cpp
        UI/ADSRDisplay.cpp
        UI/MainTab.cpp
        UI/FilterTab.cpp
        UI/SettingsTab.cpp
        UI/AIAssistTab.cpp
        UI/MCEClient.cpp
)

target_include_directories(Collonka
    PRIVATE
        ${CMAKE_SOURCE_DIR}/Source
        ${CMAKE_SOURCE_DIR}/DSP
        ${CMAKE_SOURCE_DIR}/UI
)

target_compile_definitions(Collonka
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

# --- Collatz wavetable banks: 21 .bin files baked into plugin via JUCE binary data ---
juce_add_binary_data(CollonkaResources
    NAMESPACE BinaryData
    HEADER_NAME BinaryData.h
    SOURCES
        resources/collatz_K3_rule0.bin resources/collatz_K3_rule1.bin resources/collatz_K3_rule2.bin
        resources/collatz_K4_rule0.bin resources/collatz_K4_rule1.bin resources/collatz_K4_rule2.bin
        resources/collatz_K5_rule0.bin resources/collatz_K5_rule1.bin resources/collatz_K5_rule2.bin
        resources/collatz_K6_rule0.bin resources/collatz_K6_rule1.bin resources/collatz_K6_rule2.bin
        resources/collatz_K7_rule0.bin resources/collatz_K7_rule1.bin resources/collatz_K7_rule2.bin
        resources/collatz_K8_rule0.bin resources/collatz_K8_rule1.bin resources/collatz_K8_rule2.bin
        resources/collatz_K9_rule0.bin resources/collatz_K9_rule1.bin resources/collatz_K9_rule2.bin
)

target_link_libraries(Collonka
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        CollonkaResources
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Windows: Remove /MP to prevent heap exhaustion on low-memory systems
if(MSVC)
    get_target_property(_juce_warn_opts juce_recommended_warning_flags INTERFACE_COMPILE_OPTIONS)
    if(_juce_warn_opts)
        list(FILTER _juce_warn_opts EXCLUDE REGEX "/MP")
        set_target_properties(juce_recommended_warning_flags PROPERTIES INTERFACE_COMPILE_OPTIONS "${_juce_warn_opts}")
    endif()
endif()
