#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "BWColours.h"

/**
 * HeaderBar — Always-visible top strip.
 *
 * Layout (left to right):
 *   [COLLONKA logo/title] [<] [Preset Name] [>] [SAVE][INIT]
 *   [MAIN][FILTER][SETTINGS][AI ASSIST]  [output meter]
 */
class HeaderBar : public juce::Component
{
public:
    HeaderBar();

    void resized() override;
    void paint(juce::Graphics&) override;

    /** Callback when a tab button is clicked. Index 0-3. */
    std::function<void(int)> onTabChanged;

    /** Callback when preset prev/next is clicked. -1 = prev, +1 = next. */
    std::function<void(int)> onPresetNav;

    /** Callback when preset name area is clicked (opens browser). */
    std::function<void()> onPresetBrowserOpen;

    /** Callback when SAVE is clicked. */
    std::function<void()> onSave;

    /** Callback when INIT is clicked. */
    std::function<void()> onInit;

    void setPresetName(const juce::String& name);
    void setActiveTab(int tabIndex);

    /** Update output meter levels (call from timer, not audio thread). */
    void setMeterLevels(float leftDB, float rightDB);

private:
    // Title
    juce::Label titleLabel;

    // Preset nav
    juce::TextButton prevBtn { "<" };
    juce::TextButton nextBtn { ">" };
    juce::TextButton presetNameBtn { "-- Init --" };
    juce::TextButton saveBtn { "SAVE" };
    juce::TextButton initBtn { "INIT" };

    // Tab buttons
    juce::TextButton tabButtons[4];
    int activeTab = 0;

    // Meter levels
    float meterL = -60.0f, meterR = -60.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeaderBar)
};
