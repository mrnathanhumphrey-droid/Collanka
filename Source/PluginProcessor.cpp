#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginParameters.h"
#include "BinaryData.h"

// =============================================================================
// Constructor / Destructor
// =============================================================================

CollonkaAudioProcessor::CollonkaAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "CollonkaState", createParameterLayout())
{
}

CollonkaAudioProcessor::~CollonkaAudioProcessor() = default;

// =============================================================================
// Parameter Layout
// =============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
CollonkaAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Helper: waveform choices
    juce::StringArray waveformChoices { "Sine", "Triangle", "Sawtooth", "Reverse Saw", "Square", "Pulse" };
    juce::StringArray rangeChoices { "32'", "16'", "8'", "4'", "2'" };
    juce::StringArray osc3ModeChoices { "Oscillator", "LFO" };
    juce::StringArray noiseTypeChoices { "White", "Pink" };
    juce::StringArray keyTrackChoices { "0%", "33%", "67%", "100%" };
    juce::StringArray slopeChoices { "12 dB/oct", "24 dB/oct" };
    juce::StringArray lfoWaveChoices { "Triangle", "Square", "Sine" };
    juce::StringArray lfoTargetChoices {
        "Filter Cutoff", "Pitch", "Amplitude", "Drive",
        "WT POS", "Collatz K", "Formant Q", "Formant Wet", "Formant Anchor"
    };
    juce::StringArray bassModeChoices { "Pluck", "808", "Reese" };

    // --- OSC1 ---
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::osc1Waveform, 1), "OSC1 Waveform", waveformChoices, 2));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::osc1Range, 1), "OSC1 Range", rangeChoices, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::osc1Level, 1), "OSC1 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID(ParamIDs::osc1TuneSemi, 1), "OSC1 Tune Semi", -24, 24, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::osc1TuneCents, 1), "OSC1 Fine Tune",
        juce::NormalisableRange<float>(-100.0f, 100.0f), 0.0f));

    // --- OSC2 ---
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::osc2Waveform, 1), "OSC2 Waveform", waveformChoices, 2));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::osc2Range, 1), "OSC2 Range", rangeChoices, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::osc2Level, 1), "OSC2 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::osc2Detune, 1), "OSC2 Detune",
        juce::NormalisableRange<float>(-100.0f, 100.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID(ParamIDs::osc2TuneSemi, 1), "OSC2 Tune Semi", -24, 24, 0));

    // --- OSC3 ---
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::osc3Mode, 1), "OSC3 Mode", osc3ModeChoices, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::osc3Waveform, 1), "OSC3 Waveform", waveformChoices, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::osc3Range, 1), "OSC3 Range", rangeChoices, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::osc3Level, 1), "OSC3 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID(ParamIDs::osc3TuneSemi, 1), "OSC3 Tune Semi", -24, 24, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::osc3LFORate, 1), "OSC3 LFO Rate",
        juce::NormalisableRange<float>(0.1f, 20.0f, 0.0f, 0.4f), 1.0f));

    // --- Noise ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::noiseLevel, 1), "Noise Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::noiseType, 1), "Noise Type", noiseTypeChoices, 0));

    // --- Mixer ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::feedbackAmount, 1), "Feedback",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // --- Filter ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::filterCutoff, 1), "Filter Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.3f), 2000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::filterResonance, 1), "Filter Resonance",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::filterEnvAmount, 1), "Filter Env Amount",
        juce::NormalisableRange<float>(-1.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::filterKeyTrack, 1), "Filter Key Track", keyTrackChoices, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::filterSlope, 1), "Filter Slope", slopeChoices, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::filterSaturation, 1), "Filter Saturation",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.2f));

    // --- Filter Envelope ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::envFAttack, 1), "Filter Env Attack",
        juce::NormalisableRange<float>(0.0f, 5.0f, 0.0f, 0.3f), 0.001f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::envFDecay, 1), "Filter Env Decay",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.0f, 0.3f), 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::envFSustain, 1), "Filter Env Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::envFRelease, 1), "Filter Env Release",
        juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.3f), 0.1f));

    // --- Amplitude Envelope ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::envAAttack, 1), "Amp Env Attack",
        juce::NormalisableRange<float>(0.0f, 5.0f, 0.0f, 0.3f), 0.001f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::envADecay, 1), "Amp Env Decay",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.0f, 0.3f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::envASustain, 1), "Amp Env Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::envARelease, 1), "Amp Env Release",
        juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.3f), 0.2f));

    // --- Pitch Envelope ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::pitchEnvAmount, 1), "Pitch Env Amount",
        juce::NormalisableRange<float>(0.0f, 24.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::pitchEnvTime, 1), "Pitch Env Time",
        juce::NormalisableRange<float>(0.001f, 0.5f, 0.0f, 0.4f), 0.05f));

    // --- Glide ---
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(ParamIDs::glideOn, 1), "Glide On", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::glideTime, 1), "Glide Time",
        juce::NormalisableRange<float>(0.0f, 0.5f, 0.0f, 0.5f), 0.08f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(ParamIDs::glideLegato, 1), "Glide Legato", true));

    // --- LFO ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::lfoRate, 1), "LFO Rate",
        juce::NormalisableRange<float>(0.01f, 20.0f, 0.0f, 0.4f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::lfoDepth, 1), "LFO Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::lfoWaveform, 1), "LFO Waveform", lfoWaveChoices, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::lfoTarget, 1), "LFO Target", lfoTargetChoices, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(ParamIDs::lfoSync, 1), "LFO Sync", false));

    // --- Drive Mode ---
    juce::StringArray driveModeChoices { "Auto", "Tape", "Sub Harmonic", "Transient", "Symmetric" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::driveMode, 1), "Drive Mode", driveModeChoices, 0));

    // --- Tape Saturation / Driver ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::tapeDrive, 1), "Tape Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::tapeSaturation, 1), "Tape Saturation",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::tapeBump, 1), "Tape Bump",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::tapeMix, 1), "Tape Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    // --- Output ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::driveAmount, 1), "Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    // --- BillyWonka Bass EQ ---
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(ParamIDs::eqEnabled, 1), "EQ On", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::eqHPFFreq, 1), "HPF Freq",
        juce::NormalisableRange<float>(20.0f, 80.0f, 0.0f, 0.5f), 30.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::eqSubFreq, 1), "Sub Freq",
        juce::NormalisableRange<float>(30.0f, 120.0f, 0.0f, 0.5f), 60.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::eqSubGain, 1), "Sub Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f), 3.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::eqFundFreq, 1), "Fund Freq",
        juce::NormalisableRange<float>(60.0f, 250.0f, 0.0f, 0.5f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::eqFundGain, 1), "Fund Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f), 2.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::eqFundQ, 1), "Fund Q",
        juce::NormalisableRange<float>(0.5f, 4.0f), 1.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::eqMudFreq, 1), "Mud Freq",
        juce::NormalisableRange<float>(150.0f, 600.0f, 0.0f, 0.5f), 280.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::eqMudGain, 1), "Mud Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f), -3.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::eqMudQ, 1), "Mud Q",
        juce::NormalisableRange<float>(0.5f, 3.0f), 1.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::stereoWidth, 1), "Stereo Width",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::masterVolume, 1), "Master Volume",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));

    // --- Compressor ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::compThreshold, 1), "Comp Threshold",
        juce::NormalisableRange<float>(-30.0f, 0.0f), -12.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::compAttack, 1), "Comp Attack",
        juce::NormalisableRange<float>(10.0f, 120.0f), 60.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::compRelease, 1), "Comp Release",
        juce::NormalisableRange<float>(20.0f, 300.0f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::compOpticalRatio, 1), "Comp Optical Ratio",
        juce::NormalisableRange<float>(4.0f, 8.0f), 6.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::compParallelMix, 1), "Comp Parallel Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.45f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::compOutputGain, 1), "Comp Output Gain",
        juce::NormalisableRange<float>(-6.0f, 12.0f), 3.0f));

    // --- Bass Reverb ---
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(ParamIDs::reverbEnabled, 1), "Reverb On", false));
    juce::StringArray reverbModeChoices { "Room", "Plate", "Spring", "Hall" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::reverbMode, 1), "Reverb Mode", reverbModeChoices, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::reverbMix, 1), "Reverb Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::reverbDecay, 1), "Reverb Decay",
        juce::NormalisableRange<float>(0.1f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::reverbTone, 1), "Reverb Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    // --- Bass Mode ---
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::bassMode, 1), "Bass Mode", bassModeChoices, 0));

    // --- Collatz Wavetable ---
    juce::StringArray collatzKChoices { "3", "4", "5", "6", "7", "8", "9" };
    juce::StringArray collatzRuleChoices { "K_k Spectrum", "alpha_det Phase", "Parity" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::collatzK, 1), "Collatz K", collatzKChoices, 3));  // default index 3 = K=6
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::collatzWavetableRule, 1), "Collatz Rule", collatzRuleChoices, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::wtPos, 1), "WT POS",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // --- Collatz Formant ---
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(ParamIDs::formantEnabled, 1), "Formant On", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::formantQ, 1), "Formant Q",
        juce::NormalisableRange<float>(5.0f, 30.0f), 18.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::formantWet, 1), "Formant Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParamIDs::formantAnchorHz, 1), "Formant Anchor",
        juce::NormalisableRange<float>(200.0f, 800.0f), 500.0f));

    return { params.begin(), params.end() };
}

// =============================================================================
// Prepare / Release
// =============================================================================

void CollonkaAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    synthEngine.prepare(sampleRate, samplesPerBlock);
    currentSampleRate = sampleRate;

    smoothedCutoff.reset(sampleRate, 0.01);   // 10ms smoothing
    smoothedResonance.reset(sampleRate, 0.01);
    smoothedMasterVol.reset(sampleRate, 0.01);

    // Push initial parameter state to engine
    updateEngineParameters();
}

void CollonkaAudioProcessor::releaseResources()
{
    synthEngine.reset();
}

void CollonkaAudioProcessor::stopAIPlayback()
{
    // Kill staged events so audio thread stops consuming
    injectorPlaying = false;
    stagedIndex = stagedCount;
    injectorSampleClock = 0;
    midiInjector.clear();
    midiInjector.endSequence();

    // Send note-off for all 128 notes to kill any hanging sound
    for (int n = 0; n < 128; ++n)
        synthEngine.handleNoteOff(n);
}

bool CollonkaAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

// =============================================================================
// Process Block
// =============================================================================

void CollonkaAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                   juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels beyond what we write
    for (auto i = 0; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Inject MIDI from on-screen keyboard into the buffer
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

    // --- Inject AI-generated MIDI from MidiInjector ---
    {
        int blockSamples = buffer.getNumSamples();

        // Check if a new sequence just started — drain FIFO into staging buffer
        if (midiInjector.isPlaying() && !injectorPlaying)
        {
            injectorPlaying = true;
            injectorSampleClock = 0;
            stagedCount = 0;
            stagedIndex = 0;

            MidiInjector::MidiEvent evt;
            while (stagedCount < MAX_STAGED && midiInjector.pop(evt))
                stagedEvents[stagedCount++] = evt;
        }

        // Process staged events whose time falls within this block
        if (injectorPlaying)
        {
            int blockStartSample = injectorSampleClock;
            int blockEndSample = injectorSampleClock + blockSamples;

            while (stagedIndex < stagedCount)
            {
                auto& evt = stagedEvents[stagedIndex];
                int eventSample = static_cast<int>(
                    (static_cast<double>(evt.deltaMs) / 1000.0) * currentSampleRate);

                if (eventSample >= blockEndSample)
                    break;  // This event is in a future block — stop here

                int offsetInBlock = eventSample - blockStartSample;
                if (offsetInBlock < 0) offsetInBlock = 0;
                if (offsetInBlock >= blockSamples) offsetInBlock = blockSamples - 1;

                if (evt.velocity > 0)
                    midiMessages.addEvent(
                        juce::MidiMessage::noteOn(1, static_cast<int>(evt.note),
                                                   static_cast<juce::uint8>(evt.velocity)),
                        offsetInBlock);
                else
                    midiMessages.addEvent(
                        juce::MidiMessage::noteOff(1, static_cast<int>(evt.note)),
                        offsetInBlock);

                ++stagedIndex;
            }

            injectorSampleClock += blockSamples;

            // End playback when all staged events have been consumed
            if (stagedIndex >= stagedCount)
            {
                injectorPlaying = false;
                injectorSampleClock = 0;
                midiInjector.endSequence();
            }
        }
    }

    // --- Parse MIDI ---
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();

        if (msg.isNoteOn())
        {
            synthEngine.handleNoteOn(msg.getNoteNumber(),
                                     msg.getFloatVelocity());
        }
        else if (msg.isNoteOff())
        {
            synthEngine.handleNoteOff(msg.getNoteNumber());
        }
        else if (msg.isPitchWheel())
        {
            synthEngine.handlePitchBend(msg.getPitchWheelValue());
        }
        else if (msg.isController())
        {
            synthEngine.handleControlChange(msg.getControllerNumber(),
                                            msg.getControllerValue());
        }
    }

    // --- Update parameters from APVTS ---
    updateEngineParameters();

    // --- Render audio ---
    int numSamples = buffer.getNumSamples();

    if (totalNumOutputChannels >= 2)
    {
        float* leftChannel = buffer.getWritePointer(0);
        float* rightChannel = buffer.getWritePointer(1);
        synthEngine.process(leftChannel, rightChannel, numSamples);
    }
    else if (totalNumOutputChannels == 1)
    {
        // Mono: render to a temp stereo pair and take the left channel
        float* monoChannel = buffer.getWritePointer(0);
        // Use the mono buffer for both left and right — engine writes mono anyway
        synthEngine.process(monoChannel, monoChannel, numSamples);
    }
}

// =============================================================================
// Parameter Update — pull from APVTS, push to SynthEngine
// =============================================================================

void CollonkaAudioProcessor::updateEngineParameters()
{
    // Helper lambdas to retrieve parameter values
    auto getFloat = [this](const char* id) {
        return apvts.getRawParameterValue(id)->load();
    };
    auto getChoice = [this](const char* id) -> int {
        return static_cast<int>(apvts.getRawParameterValue(id)->load());
    };
    auto getBool = [this](const char* id) -> bool {
        return apvts.getRawParameterValue(id)->load() > 0.5f;
    };

    // --- Bass Mode (check first — it may override other params) ---
    int mode = getChoice(ParamIDs::bassMode);
    synthEngine.setBassMode(static_cast<SynthEngine::BassMode>(mode));

    // --- OSC1 ---
    synthEngine.setOsc1Waveform(getChoice(ParamIDs::osc1Waveform));
    synthEngine.setOsc1Range(getChoice(ParamIDs::osc1Range));
    synthEngine.setOsc1Level(getFloat(ParamIDs::osc1Level));
    synthEngine.setOsc1TuneSemi(static_cast<int>(getFloat(ParamIDs::osc1TuneSemi)));
    synthEngine.setOsc1TuneCents(getFloat(ParamIDs::osc1TuneCents));

    // --- OSC2 ---
    synthEngine.setOsc2Waveform(getChoice(ParamIDs::osc2Waveform));
    synthEngine.setOsc2Range(getChoice(ParamIDs::osc2Range));
    synthEngine.setOsc2Level(getFloat(ParamIDs::osc2Level));
    synthEngine.setOsc2Detune(getFloat(ParamIDs::osc2Detune));
    synthEngine.setOsc2TuneSemi(static_cast<int>(getFloat(ParamIDs::osc2TuneSemi)));

    // --- OSC3 ---
    synthEngine.setOsc3Mode(getChoice(ParamIDs::osc3Mode));
    synthEngine.setOsc3Waveform(getChoice(ParamIDs::osc3Waveform));
    synthEngine.setOsc3Range(getChoice(ParamIDs::osc3Range));
    synthEngine.setOsc3Level(getFloat(ParamIDs::osc3Level));
    synthEngine.setOsc3TuneSemi(static_cast<int>(getFloat(ParamIDs::osc3TuneSemi)));
    synthEngine.setOsc3LFORate(getFloat(ParamIDs::osc3LFORate));

    // --- Noise ---
    synthEngine.setNoiseLevel(getFloat(ParamIDs::noiseLevel));
    synthEngine.setNoiseType(getChoice(ParamIDs::noiseType));

    // --- Mixer ---
    synthEngine.setFeedbackAmount(getFloat(ParamIDs::feedbackAmount));

    // --- Filter ---
    synthEngine.setFilterCutoff(getFloat(ParamIDs::filterCutoff));
    synthEngine.setFilterResonance(getFloat(ParamIDs::filterResonance));
    synthEngine.setFilterEnvAmount(getFloat(ParamIDs::filterEnvAmount));
    synthEngine.setFilterKeyTrack(getChoice(ParamIDs::filterKeyTrack));
    synthEngine.setFilterSlope(getChoice(ParamIDs::filterSlope));
    synthEngine.setFilterSaturation(getFloat(ParamIDs::filterSaturation));

    // --- Filter Envelope ---
    synthEngine.setFilterEnvAttack(getFloat(ParamIDs::envFAttack));
    synthEngine.setFilterEnvDecay(getFloat(ParamIDs::envFDecay));
    synthEngine.setFilterEnvSustain(getFloat(ParamIDs::envFSustain));
    synthEngine.setFilterEnvRelease(getFloat(ParamIDs::envFRelease));

    // --- Amplitude Envelope ---
    synthEngine.setAmpEnvAttack(getFloat(ParamIDs::envAAttack));
    synthEngine.setAmpEnvDecay(getFloat(ParamIDs::envADecay));
    synthEngine.setAmpEnvSustain(getFloat(ParamIDs::envASustain));
    synthEngine.setAmpEnvRelease(getFloat(ParamIDs::envARelease));

    // --- Pitch Envelope ---
    synthEngine.setPitchEnvAmount(getFloat(ParamIDs::pitchEnvAmount));
    synthEngine.setPitchEnvTime(getFloat(ParamIDs::pitchEnvTime));

    // --- Glide ---
    synthEngine.setGlideEnabled(getBool(ParamIDs::glideOn));
    synthEngine.setGlideTime(getFloat(ParamIDs::glideTime));
    synthEngine.setGlideLegato(getBool(ParamIDs::glideLegato));

    // --- LFO ---
    synthEngine.setLFORate(getFloat(ParamIDs::lfoRate));
    synthEngine.setLFODepth(getFloat(ParamIDs::lfoDepth));
    synthEngine.setLFOWaveform(getChoice(ParamIDs::lfoWaveform));
    synthEngine.setLFOTarget(getChoice(ParamIDs::lfoTarget));
    synthEngine.setLFOSync(getBool(ParamIDs::lfoSync));

    // --- Drive Mode ---
    synthEngine.setDriveMode(getChoice(ParamIDs::driveMode));

    // --- Tape Saturation / Driver ---
    synthEngine.setTapeDrive(getFloat(ParamIDs::tapeDrive));
    synthEngine.setTapeSaturation(getFloat(ParamIDs::tapeSaturation));
    synthEngine.setTapeBump(getFloat(ParamIDs::tapeBump));
    synthEngine.setTapeMix(getFloat(ParamIDs::tapeMix));

    // --- Output ---
    synthEngine.setDrive(getFloat(ParamIDs::driveAmount));
    // --- BillyWonka Bass EQ ---
    synthEngine.setEQEnabled(getBool(ParamIDs::eqEnabled));
    synthEngine.setHPFFreq(getFloat(ParamIDs::eqHPFFreq));
    synthEngine.setSubFreq(getFloat(ParamIDs::eqSubFreq));
    synthEngine.setSubGain(getFloat(ParamIDs::eqSubGain));
    synthEngine.setFundFreq(getFloat(ParamIDs::eqFundFreq));
    synthEngine.setFundGain(getFloat(ParamIDs::eqFundGain));
    synthEngine.setFundQ(getFloat(ParamIDs::eqFundQ));
    synthEngine.setMudFreq(getFloat(ParamIDs::eqMudFreq));
    synthEngine.setMudGain(getFloat(ParamIDs::eqMudGain));
    synthEngine.setMudQ(getFloat(ParamIDs::eqMudQ));
    synthEngine.setStereoWidth(getFloat(ParamIDs::stereoWidth));
    synthEngine.setMasterVolume(getFloat(ParamIDs::masterVolume));

    // --- Compressor ---
    synthEngine.setCompThreshold(getFloat(ParamIDs::compThreshold));
    synthEngine.setCompTransientAttack(getFloat(ParamIDs::compAttack));
    synthEngine.setCompTransientRelease(getFloat(ParamIDs::compRelease));
    synthEngine.setCompOpticalRatio(getFloat(ParamIDs::compOpticalRatio));
    synthEngine.setCompParallelMix(getFloat(ParamIDs::compParallelMix));
    synthEngine.setCompOutputGain(getFloat(ParamIDs::compOutputGain));

    // --- Bass Reverb ---
    synthEngine.setReverbEnabled(getBool(ParamIDs::reverbEnabled));
    synthEngine.setReverbMode(getChoice(ParamIDs::reverbMode));
    synthEngine.setReverbMix(getFloat(ParamIDs::reverbMix));
    synthEngine.setReverbDecay(getFloat(ParamIDs::reverbDecay));
    synthEngine.setReverbTone(getFloat(ParamIDs::reverbTone));

    // --- Collatz Wavetable ---
    int collatzKIdx = getChoice(ParamIDs::collatzK);                // 0..6 -> K = 3..9
    int kValue      = collatzKIdx + 3;
    int ruleIdx     = getChoice(ParamIDs::collatzWavetableRule);    // 0..2

    reloadCollatzBankIfNeeded(kValue, ruleIdx);
    synthEngine.setCollatzK(kValue);
    synthEngine.setWtPos(getFloat(ParamIDs::wtPos));

    // --- Collatz Formant ---
    synthEngine.setFormantEnabled(getBool(ParamIDs::formantEnabled));
    synthEngine.setFormantQ(getFloat(ParamIDs::formantQ));
    synthEngine.setFormantWet(getFloat(ParamIDs::formantWet));
    synthEngine.setFormantAnchorHz(getFloat(ParamIDs::formantAnchorHz));
}

void CollonkaAudioProcessor::reloadCollatzBankIfNeeded(int k, int rule)
{
    if (k == currentBankK && rule == currentBankRule && currentBankPtr != nullptr)
        return;

    juce::String resourceName;
    resourceName << "collatz_K" << k << "_rule" << rule << "_bin";

    int sizeBytes = 0;
    const char* data = BinaryData::getNamedResource(resourceName.toRawUTF8(), sizeBytes);

    if (data != nullptr && sizeBytes > 0)
    {
        currentBankPtr  = reinterpret_cast<const float*>(data);
        currentBankK    = k;
        currentBankRule = rule;
        synthEngine.setCollatzBank(currentBankPtr);
    }
    // If lookup failed, leave the previous bank pointer in place so audio still plays.
}

// =============================================================================
// Plugin Info
// =============================================================================

const juce::String CollonkaAudioProcessor::getName() const { return JucePlugin_Name; }
bool CollonkaAudioProcessor::acceptsMidi() const { return true; }
bool CollonkaAudioProcessor::producesMidi() const { return false; }
bool CollonkaAudioProcessor::isMidiEffect() const { return false; }
double CollonkaAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int CollonkaAudioProcessor::getNumPrograms() { return 1; }
int CollonkaAudioProcessor::getCurrentProgram() { return 0; }
void CollonkaAudioProcessor::setCurrentProgram(int) {}
const juce::String CollonkaAudioProcessor::getProgramName(int) { return {}; }
void CollonkaAudioProcessor::changeProgramName(int, const juce::String&) {}

bool CollonkaAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* CollonkaAudioProcessor::createEditor()
{
    return new CollonkaAudioProcessorEditor(*this);
}

// =============================================================================
// State Save / Load (preset management)
// =============================================================================

void CollonkaAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void CollonkaAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        updateEngineParameters();
    }
}

// =============================================================================
// Plugin Instantiation
// =============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CollonkaAudioProcessor();
}
