#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginParameters.h"
#include "SynthEngine.h"
#include "MidiInjector.h"

/**
 * PluginProcessor — JUCE AudioProcessor for Collonka.
 *
 * Bridges the JUCE framework with the pure-C++ SynthEngine:
 * - Owns the AudioProcessorValueTreeState (APVTS) for all parameters.
 * - Parses MIDI in processBlock() and forwards to SynthEngine.
 * - Pulls parameter values each block and pushes to SynthEngine.
 * - Handles preset save/load via getStateInformation/setStateInformation.
 */
class CollonkaAudioProcessor : public juce::AudioProcessor
{
public:
    CollonkaAudioProcessor();
    ~CollonkaAudioProcessor() override;

    // --- AudioProcessor interface ---
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    /** Public access to the parameter tree for the editor. */
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    /** Public access to keyboard state for the on-screen MIDI keyboard. */
    juce::MidiKeyboardState& getKeyboardState() { return keyboardState; }

    /** Public access to MIDI injector for AI-generated note sequences. */
    MidiInjector& getMidiInjector() { return midiInjector; }

    /** Stop AI playback and send all-notes-off (MIDI panic). */
    void stopAIPlayback();

private:
    /** Create the APVTS parameter layout with all synth parameters. */
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /** Pull all current parameter values from APVTS and push to SynthEngine. */
    void updateEngineParameters();

    // The synthesis engine (pure C++, no JUCE dependency)
    SynthEngine synthEngine;

    // JUCE parameter system
    juce::AudioProcessorValueTreeState apvts;

    // On-screen keyboard state (shared with editor)
    juce::MidiKeyboardState keyboardState;

    // AI MIDI injection (lock-free SPSC queue: message thread → audio thread)
    MidiInjector midiInjector;
    double currentSampleRate = 44100.0;
    int injectorSampleClock = 0;    // Running sample counter for sequence timing
    bool injectorPlaying = false;

    // Staging buffer: events popped from FIFO, sorted by time, consumed per-block
    static constexpr int MAX_STAGED = 256;
    MidiInjector::MidiEvent stagedEvents[MAX_STAGED] = {};
    int stagedCount = 0;
    int stagedIndex = 0;  // Next event to process

    // Smoothed parameter values (JUCE SmoothedValue for zipper-free changes)
    juce::SmoothedValue<float> smoothedCutoff;
    juce::SmoothedValue<float> smoothedResonance;
    juce::SmoothedValue<float> smoothedMasterVol;

    // Collatz bank loader state — track current k/rule so we only re-look-up on change.
    int          currentBankK    = -1;
    int          currentBankRule = -1;
    const float* currentBankPtr  = nullptr;
    void reloadCollatzBankIfNeeded(int k, int rule);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CollonkaAudioProcessor)
};
