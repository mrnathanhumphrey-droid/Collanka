#include "HeaderBar.h"

HeaderBar::HeaderBar()
{
    // --- Title ---
    titleLabel.setText("COLLONKA", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(18.0f, juce::Font::bold).withExtraKerningFactor(0.2f));
    titleLabel.setColour(juce::Label::textColourId, BW::Pink);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    // --- Preset nav ---
    auto setupSmallBtn = [](juce::TextButton& btn)
    {
        btn.setColour(juce::TextButton::buttonColourId, BW::Deep);
        btn.setColour(juce::TextButton::textColourOffId, BW::PurpleGlow);
    };
    setupSmallBtn(prevBtn);
    setupSmallBtn(nextBtn);
    prevBtn.onClick = [this]() { if (onPresetNav) onPresetNav(-1); };
    nextBtn.onClick = [this]() { if (onPresetNav) onPresetNav(+1); };
    addAndMakeVisible(prevBtn);
    addAndMakeVisible(nextBtn);

    presetNameBtn.setColour(juce::TextButton::buttonColourId, BW::Deep);
    presetNameBtn.setColour(juce::TextButton::textColourOffId, BW::White);
    presetNameBtn.onClick = [this]() { if (onPresetBrowserOpen) onPresetBrowserOpen(); };
    addAndMakeVisible(presetNameBtn);

    // Save / Init
    saveBtn.setColour(juce::TextButton::buttonColourId, BW::PurpleDark);
    saveBtn.setColour(juce::TextButton::textColourOffId, BW::PurpleGlow);
    saveBtn.onClick = [this]() { if (onSave) onSave(); };
    addAndMakeVisible(saveBtn);

    initBtn.setColour(juce::TextButton::buttonColourId, BW::PurpleDark);
    initBtn.setColour(juce::TextButton::textColourOffId, BW::Grey);
    initBtn.onClick = [this]() { if (onInit) onInit(); };
    addAndMakeVisible(initBtn);

    // --- Tab buttons ---
    const juce::String tabNames[] = { "MAIN", "FILTER", "SETTINGS", "AI ASSIST" };
    for (int i = 0; i < 4; ++i)
    {
        tabButtons[i].setButtonText(tabNames[i]);
        tabButtons[i].setClickingTogglesState(true);
        tabButtons[i].setRadioGroupId(1001);
        tabButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        tabButtons[i].setColour(juce::TextButton::textColourOffId, BW::TextMuted);
        tabButtons[i].setColour(juce::TextButton::textColourOnId, BW::Pink);
        tabButtons[i].onClick = [this, i]()
        {
            activeTab = i;
            if (onTabChanged) onTabChanged(i);
            repaint();
        };
        addAndMakeVisible(tabButtons[i]);
    }
    tabButtons[0].setToggleState(true, juce::dontSendNotification);
}

void HeaderBar::setPresetName(const juce::String& name)
{
    presetNameBtn.setButtonText(name);
}

void HeaderBar::setActiveTab(int tabIndex)
{
    if (tabIndex >= 0 && tabIndex < 4)
    {
        activeTab = tabIndex;
        tabButtons[tabIndex].setToggleState(true, juce::dontSendNotification);
        repaint();
    }
}

void HeaderBar::setMeterLevels(float leftDB, float rightDB)
{
    meterL = leftDB;
    meterR = rightDB;
    repaint(); // Only the meter area ideally, but fine for now
}

void HeaderBar::paint(juce::Graphics& g)
{
    // Background
    g.setColour(BW::Deep);
    g.fillRect(getLocalBounds());

    // Bottom border
    g.setColour(BW::Grey.withAlpha(0.4f));
    g.fillRect(0, getHeight() - 1, getWidth(), 1);

    // Active tab underline
    for (int i = 0; i < 4; ++i)
    {
        if (tabButtons[i].getToggleState())
        {
            auto tabBounds = tabButtons[i].getBounds();
            g.setColour(BW::Pink);
            g.fillRect(tabBounds.getX(), tabBounds.getBottom() - 2,
                       tabBounds.getWidth(), 2);
        }
    }

    // --- Output meter (right side) ---
    auto meterArea = getLocalBounds().removeFromRight(30).reduced(4, 8);
    int meterW = 4;
    auto meterL_area = meterArea.removeFromLeft(meterW);
    meterArea.removeFromLeft(3);
    auto meterR_area = meterArea.removeFromLeft(meterW);

    auto drawMeter = [&](juce::Rectangle<int> area, float dB)
    {
        g.setColour(BW::Grey.withAlpha(0.3f));
        g.fillRoundedRectangle(area.toFloat(), 1.5f);

        // Map dB to 0-1 (range -60 to 0 dB)
        float level = juce::jlimit(0.0f, 1.0f, (dB + 60.0f) / 60.0f);
        int filledH = static_cast<int>(area.getHeight() * level);
        auto filledArea = area.withTop(area.getBottom() - filledH);

        // Gradient: green → pink near top
        g.setColour(level > 0.85f ? BW::Pink : BW::PurpleGlow);
        g.fillRoundedRectangle(filledArea.toFloat(), 1.5f);
    };

    drawMeter(meterL_area, meterL);
    drawMeter(meterR_area, meterR);
}

void HeaderBar::resized()
{
    auto bounds = getLocalBounds();
    auto topRow = bounds.removeFromTop(28).reduced(8, 2);
    auto tabRow = bounds.reduced(8, 0);

    // Top row: title | preset nav | save/init
    titleLabel.setBounds(topRow.removeFromLeft(80));
    topRow.removeFromLeft(8);

    // Save/Init on far right
    initBtn.setBounds(topRow.removeFromRight(40));
    topRow.removeFromRight(4);
    saveBtn.setBounds(topRow.removeFromRight(46));
    topRow.removeFromRight(12);

    // Meter space (already painted manually on the right)
    topRow.removeFromRight(30);

    // Preset nav in remaining centre
    prevBtn.setBounds(topRow.removeFromLeft(24));
    topRow.removeFromLeft(2);
    nextBtn.setBounds(topRow.removeFromRight(24));
    topRow.removeFromRight(2);
    presetNameBtn.setBounds(topRow);

    // Tab row
    int tabW = juce::jmin(90, tabRow.getWidth() / 4);
    for (int i = 0; i < 4; ++i)
        tabButtons[i].setBounds(tabRow.removeFromLeft(tabW));
}
