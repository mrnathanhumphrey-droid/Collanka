#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "BWLookAndFeel.h"
#include "HeaderBar.h"
#include "MainTab.h"
#include "FilterTab.h"
#include "SettingsTab.h"
#include "AIAssistTab.h"
#include "MCEClient.h"

/**
 * PluginEditor — Collonka main editor window.
 *
 * Header bar (always visible) + 4 tabbed content panels + MIDI keyboard.
 */
class CollonkaAudioProcessorEditor : public juce::AudioProcessorEditor,
                                            public juce::Timer
{
public:
    explicit CollonkaAudioProcessorEditor(CollonkaAudioProcessor&);
    ~CollonkaAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    void showTab(int index);
    void loadPresetByIndex(int index);
    void scanPresets();

    CollonkaAudioProcessor& processorRef;

    BWLookAndFeel bwLookAndFeel;

    // Header
    HeaderBar headerBar;

    // Tab content panels
    MainTab mainTab;
    FilterTab filterTab;
    SettingsTab settingsTab;
    AIAssistTab aiAssistTab;
    int currentTab = 0;

    // MIDI keyboard at bottom
    juce::MidiKeyboardComponent keyboardComponent;

    // MCE Client (AI integration)
    MCEClient mceClient;

    // Preset management
    juce::StringArray presetNames;
    juce::Array<juce::File> presetFiles;
    int currentPresetIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CollonkaAudioProcessorEditor)
};
