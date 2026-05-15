#include "PluginEditor.h"

CollonkaAudioProcessorEditor::CollonkaAudioProcessorEditor(
    CollonkaAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      mainTab(p.getAPVTS()),
      filterTab(p.getAPVTS()),
      settingsTab(p.getAPVTS()),
      aiAssistTab(p.getAPVTS()),
      keyboardComponent(p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard),
      mceClient(p.getAPVTS(), p.getMidiInjector())
{
    setLookAndFeel(&bwLookAndFeel);

    // --- Header ---
    headerBar.onTabChanged = [this](int idx) { showTab(idx); };
    headerBar.onPresetNav = [this](int dir) { loadPresetByIndex(currentPresetIndex + dir); };
    headerBar.onPresetBrowserOpen = [this]()
    {
        // Simple popup with preset list
        juce::PopupMenu menu;
        menu.addItem(1, "-- Init --");
        for (int i = 0; i < presetNames.size(); ++i)
            menu.addItem(i + 2, presetNames[i], true, i == currentPresetIndex);

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(headerBar),
            [this](int result)
            {
                if (result == 1)
                {
                    currentPresetIndex = -1;
                    headerBar.setPresetName("-- Init --");
                }
                else if (result >= 2)
                    loadPresetByIndex(result - 2);
            });
    };
    headerBar.onInit = [this]()
    {
        currentPresetIndex = -1;
        headerBar.setPresetName("-- Init --");
    };
    addAndMakeVisible(headerBar);

    // --- Tabs ---
    addAndMakeVisible(mainTab);
    addChildComponent(filterTab);
    addChildComponent(settingsTab);
    addChildComponent(aiAssistTab);

    // --- Keyboard ---
    keyboardComponent.setKeyWidth(28.0f);
    keyboardComponent.setScrollButtonsVisible(true);
    keyboardComponent.setAvailableRange(24, 96);
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, BW::Purple);
    keyboardComponent.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, BW::Deep.brighter(0.2f));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::blackNoteColourId, BW::Black);
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, BW::Grey);
    addAndMakeVisible(keyboardComponent);

    // --- MCE Client ---
    mceClient.onMessageReceived = [this](const juce::String& sender, const juce::String& text)
    {
        aiAssistTab.addMessage(sender, text);
    };
    mceClient.onConnectionStatusChanged = [this](bool conn, int lat)
    {
        aiAssistTab.setConnectionStatus(conn, lat);
    };
    mceClient.onParamSuggestion = [this](const juce::StringPairArray& params)
    {
        auto& apvts = processorRef.getAPVTS();
        for (auto& key : params.getAllKeys())
        {
            if (auto* param = apvts.getParameter(key))
            {
                float rawValue = params.getValue(key, "0").getFloatValue();
                float normalized = param->getNormalisableRange().convertTo0to1(rawValue);
                param->setValueNotifyingHost(normalized);
            }
        }
        aiAssistTab.addMessage("System", "Applied " + juce::String(params.size()) + " parameter changes.");
    };

    // Wire AIAssistTab callbacks to MCEClient
    aiAssistTab.onSendMessage = [this](const juce::String& msg)
    {
        mceClient.sendChatMessage(msg);
    };
    aiAssistTab.onApplySuggestion = [this]()
    {
        mceClient.requestApplySuggestion();
    };
    aiAssistTab.onPlayRequest = [this](const juce::String& desc)
    {
        mceClient.requestPlaySequence(desc);
    };
    aiAssistTab.onStopRequest = [this]()
    {
        processorRef.stopAIPlayback();
    };

    // Wire reconnect button in AIAssistTab
    // The reconnectBtn onClick needs to be set from here since MCEClient is owned by editor
    // We'll trigger connect via a lambda after the URL field changes
    mceClient.connect();

    // --- Presets ---
    scanPresets();

    setSize(900, 680);
    setResizable(true, true);
    setResizeLimits(700, 500, 1400, 1000);

    startTimerHz(30); // UI refresh for meters
}

CollonkaAudioProcessorEditor::~CollonkaAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void CollonkaAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(BW::Black);
}

void CollonkaAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Header: 54px
    headerBar.setBounds(bounds.removeFromTop(54));

    // Keyboard: 70px at bottom
    keyboardComponent.setBounds(bounds.removeFromBottom(70));

    // Tab content fills middle
    mainTab.setBounds(bounds);
    filterTab.setBounds(bounds);
    settingsTab.setBounds(bounds);
    aiAssistTab.setBounds(bounds);
}

void CollonkaAudioProcessorEditor::showTab(int index)
{
    currentTab = index;
    mainTab.setVisible(index == 0);
    filterTab.setVisible(index == 1);
    settingsTab.setVisible(index == 2);
    aiAssistTab.setVisible(index == 3);
}

void CollonkaAudioProcessorEditor::timerCallback()
{
    // Could add peak meter levels here in future
    // headerBar.setMeterLevels(leftDB, rightDB);
}

// =============================================================================
// Preset Management
// =============================================================================

void CollonkaAudioProcessorEditor::scanPresets()
{
    // Search multiple locations for the Factory presets folder
    juce::Array<juce::File> searchPaths;

    // 1. macOS: ~/Library/Application Support/Collonka/Presets/Factory (install.sh target)
    auto appSupport = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("Collonka")
                          .getChildFile("Presets")
                          .getChildFile("Factory");
    searchPaths.add(appSupport);

    // 2. Windows: %APPDATA%/Collonka/Presets/Factory (Install.bat target)
    // (same as above on Windows — userApplicationDataDirectory maps to %APPDATA%)

    // 3. Relative to executable (standalone dev builds)
    auto exeFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    searchPaths.add(exeFile.getParentDirectory()
                           .getParentDirectory()
                           .getParentDirectory()
                           .getParentDirectory()
                           .getChildFile("Presets")
                           .getChildFile("Factory"));

    // 4. Windows dev fallback
    searchPaths.add(juce::File("C:\\Collonka\\Presets\\Factory"));

    // 5. macOS: next to the .vst3/.component bundle
    searchPaths.add(exeFile.getParentDirectory()
                           .getParentDirectory()
                           .getParentDirectory()
                           .getChildFile("Presets")
                           .getChildFile("Factory"));

    // Use the first directory that exists
    juce::File presetsDir;
    for (auto& path : searchPaths)
    {
        if (path.isDirectory())
        {
            presetsDir = path;
            break;
        }
    }

    if (presetsDir.isDirectory())
    {
        auto files = presetsDir.findChildFiles(juce::File::findFiles, false, "*.xml");
        files.sort();

        for (auto& f : files)
        {
            presetFiles.add(f);
            presetNames.add(f.getFileNameWithoutExtension());
        }
    }
}

void CollonkaAudioProcessorEditor::loadPresetByIndex(int index)
{
    if (index < 0 || index >= presetFiles.size()) return;

    auto file = presetFiles[index];
    auto xml = juce::XmlDocument::parse(file.loadFileAsString());

    if (xml == nullptr || !xml->hasTagName("CollonkaState"))
        return;

    auto& apvts = processorRef.getAPVTS();

    for (auto* paramElement : xml->getChildIterator())
    {
        if (paramElement->hasTagName("PARAM"))
        {
            auto id = paramElement->getStringAttribute("id");
            auto value = paramElement->getDoubleAttribute("value");

            if (auto* param = apvts.getParameter(id))
            {
                auto range = param->getNormalisableRange();
                float normalizedValue = range.convertTo0to1(static_cast<float>(value));
                param->setValueNotifyingHost(normalizedValue);
            }
        }
    }

    currentPresetIndex = index;
    headerBar.setPresetName(presetNames[index]);
}
