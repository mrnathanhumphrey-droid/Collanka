#include "AIAssistTab.h"

static void styleCombo(juce::ComboBox& box)
{
    box.setColour(juce::ComboBox::backgroundColourId, BW::Deep);
    box.setColour(juce::ComboBox::textColourId, BW::White);
    box.setColour(juce::ComboBox::outlineColourId, BW::Grey);
}

AIAssistTab::AIAssistTab(juce::AudioProcessorValueTreeState& vts) : apvts(vts)
{
    // --- Chat display ---
    chatDisplay.setMultiLine(true);
    chatDisplay.setReadOnly(true);
    chatDisplay.setScrollbarsShown(true);
    chatDisplay.setColour(juce::TextEditor::backgroundColourId, BW::Deep);
    chatDisplay.setColour(juce::TextEditor::textColourId, BW::White);
    chatDisplay.setColour(juce::TextEditor::outlineColourId, BW::GreyBorder);
    chatDisplay.setFont(juce::Font(13.0f));
    addAndMakeVisible(chatDisplay);

    // --- Input field ---
    inputField.setMultiLine(false);
    inputField.setReturnKeyStartsNewLine(false);
    inputField.setColour(juce::TextEditor::backgroundColourId, BW::Deep);
    inputField.setColour(juce::TextEditor::textColourId, BW::White);
    inputField.setColour(juce::TextEditor::outlineColourId, BW::Grey);
    inputField.setColour(juce::TextEditor::focusedOutlineColourId, BW::Purple);
    inputField.setTextToShowWhenEmpty("Ask Claude... (type 'play', 'demo', or 'write' to generate notes)", BW::TextMuted);
    inputField.onReturnKey = [this]() { sendCurrentMessage(); };
    addAndMakeVisible(inputField);

    // --- Send button ---
    sendBtn.setColour(juce::TextButton::buttonColourId, BW::Pink);
    sendBtn.setColour(juce::TextButton::textColourOffId, BW::White);
    sendBtn.onClick = [this]() { sendCurrentMessage(); };
    addAndMakeVisible(sendBtn);

    // --- Connection status ---
    statusLabel.setFont(juce::Font(11.0f));
    statusLabel.setColour(juce::Label::textColourId, BW::Grey);
    statusLabel.setText("MCE: Offline", juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    // --- Context panel ---
    contextTitle.setText("CURRENT PRESET CONTEXT", juce::dontSendNotification);
    contextTitle.setFont(juce::Font(10.0f, juce::Font::bold).withExtraKerningFactor(0.15f));
    contextTitle.setColour(juce::Label::textColourId, BW::TextMuted);
    addAndMakeVisible(contextTitle);

    contextDisplay.setMultiLine(true);
    contextDisplay.setReadOnly(true);
    contextDisplay.setColour(juce::TextEditor::backgroundColourId, BW::PurpleDark.withAlpha(0.5f));
    contextDisplay.setColour(juce::TextEditor::textColourId, BW::PinkSoft);
    contextDisplay.setColour(juce::TextEditor::outlineColourId, BW::GreyBorder);
    contextDisplay.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.0f,
                                      juce::Font::plain));
    addAndMakeVisible(contextDisplay);

    // --- Quick prompts ---
    for (int i = 0; i < 5; ++i)
    {
        quickPrompts[i].setButtonText(promptTexts[i]);
        quickPrompts[i].setColour(juce::TextButton::buttonColourId, BW::PurpleDark);
        quickPrompts[i].setColour(juce::TextButton::textColourOffId, BW::PinkSoft);
        quickPrompts[i].onClick = [this, i]()
        {
            inputField.setText(promptTexts[i]);
            sendCurrentMessage();
        };
        addAndMakeVisible(quickPrompts[i]);
    }

    // --- Play configuration ---
    playConfigLabel.setText("COMPOSE", juce::dontSendNotification);
    playConfigLabel.setFont(juce::Font(10.0f, juce::Font::bold).withExtraKerningFactor(0.15f));
    playConfigLabel.setColour(juce::Label::textColourId, BW::Pink);
    addAndMakeVisible(playConfigLabel);

    // Key selector
    keyBox.addItem("C minor", 1);
    keyBox.addItem("D minor", 2);
    keyBox.addItem("E minor", 3);
    keyBox.addItem("F minor", 4);
    keyBox.addItem("G minor", 5);
    keyBox.addItem("A minor", 6);
    keyBox.addItem("Bb minor", 7);
    keyBox.addItem("Eb minor", 8);
    keyBox.addItem("C major", 9);
    keyBox.addItem("F major", 10);
    keyBox.addItem("G major", 11);
    keyBox.addItem("Bb major", 12);
    keyBox.setSelectedId(1, juce::dontSendNotification);
    styleCombo(keyBox);
    addAndMakeVisible(keyBox);

    // Style selector
    styleBox.addItem("R&B", 1);
    styleBox.addItem("Neo-Soul", 2);
    styleBox.addItem("Trap", 3);
    styleBox.addItem("Boom-Bap", 4);
    styleBox.addItem("Lo-Fi", 5);
    styleBox.addItem("Funk", 6);
    styleBox.addItem("Gospel", 7);
    styleBox.addItem("Afrobeats", 8);
    styleBox.setSelectedId(1, juce::dontSendNotification);
    styleCombo(styleBox);
    addAndMakeVisible(styleBox);

    // Time signature
    timeSigBox.addItem("4/4", 1);
    timeSigBox.addItem("3/4", 2);
    timeSigBox.addItem("6/8", 3);
    timeSigBox.setSelectedId(1, juce::dontSendNotification);
    styleCombo(timeSigBox);
    addAndMakeVisible(timeSigBox);

    // Bars
    barsBox.addItem("4 bars", 1);
    barsBox.addItem("8 bars", 2);
    barsBox.setSelectedId(1, juce::dontSendNotification);
    styleCombo(barsBox);
    addAndMakeVisible(barsBox);

    // BPM
    bpmBox.addItem("70 BPM", 1);
    bpmBox.addItem("80 BPM", 2);
    bpmBox.addItem("85 BPM", 3);
    bpmBox.addItem("90 BPM", 4);
    bpmBox.addItem("95 BPM", 5);
    bpmBox.addItem("100 BPM", 6);
    bpmBox.addItem("110 BPM", 7);
    bpmBox.addItem("120 BPM", 8);
    bpmBox.addItem("130 BPM", 9);
    bpmBox.addItem("140 BPM", 10);
    bpmBox.setSelectedId(4, juce::dontSendNotification);  // Default 90 BPM
    styleCombo(bpmBox);
    addAndMakeVisible(bpmBox);

    // Play button
    playBtn.setColour(juce::TextButton::buttonColourId, BW::Pink);
    playBtn.setColour(juce::TextButton::textColourOffId, BW::White);
    playBtn.onClick = [this]()
    {
        auto prompt = buildPlayPrompt();
        addMessage("You", prompt);
        if (onPlayRequest)
            onPlayRequest(prompt);
    };
    addAndMakeVisible(playBtn);

    stopBtn.setColour(juce::TextButton::buttonColourId, BW::Red);
    stopBtn.setColour(juce::TextButton::textColourOffId, BW::White);
    stopBtn.onClick = [this]()
    {
        if (onStopRequest)
            onStopRequest();
        addMessage("System", "Playback stopped.");
    };
    addAndMakeVisible(stopBtn);

    // --- Apply suggestion ---
    applyBtn.setColour(juce::TextButton::buttonColourId, BW::Purple);
    applyBtn.setColour(juce::TextButton::textColourOffId, BW::White);
    applyBtn.setEnabled(false);
    applyBtn.onClick = [this]() { if (onApplySuggestion) onApplySuggestion(); };
    addAndMakeVisible(applyBtn);

    // --- MCE URL ---
    urlLabel.setText("MCE Server:", juce::dontSendNotification);
    urlLabel.setFont(juce::Font(10.0f));
    urlLabel.setColour(juce::Label::textColourId, BW::TextMuted);
    addAndMakeVisible(urlLabel);

    urlField.setText("http://localhost:9150");
    urlField.setColour(juce::TextEditor::backgroundColourId, BW::Deep);
    urlField.setColour(juce::TextEditor::textColourId, BW::White);
    urlField.setColour(juce::TextEditor::outlineColourId, BW::Grey);
    urlField.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.0f,
                                 juce::Font::plain));
    addAndMakeVisible(urlField);

    reconnectBtn.setColour(juce::TextButton::buttonColourId, BW::PurpleDark);
    reconnectBtn.setColour(juce::TextButton::textColourOffId, BW::PurpleGlow);
    addAndMakeVisible(reconnectBtn);

    addMessage("System", "Claude AI assistant for Collonka.\nConnect to an MCE server to begin.\n\nType 'play', 'demo', or 'write' in chat to generate notes.");
}

// =============================================================================
// Message handling
// =============================================================================

void AIAssistTab::sendCurrentMessage()
{
    auto text = inputField.getText().trim();
    if (text.isEmpty()) return;

    addMessage("You", text);
    inputField.clear();

    // Route to play endpoint if trigger word detected
    if (isPlayTrigger(text))
    {
        if (onPlayRequest)
            onPlayRequest(text);
        return;
    }

    if (onSendMessage)
        onSendMessage(text);
    else
        addMessage("System", "MCE server not connected. Messages are local only.");
}

bool AIAssistTab::isPlayTrigger(const juce::String& text) const
{
    auto lower = text.toLowerCase();
    return lower.startsWith("play ") || lower.startsWith("demo ") || lower.startsWith("write ")
        || lower == "play" || lower == "demo" || lower == "write";
}

juce::String AIAssistTab::buildPlayPrompt() const
{
    auto key = keyBox.getText();
    auto style = styleBox.getText();
    auto timeSig = timeSigBox.getText();
    auto bars = barsBox.getText();
    auto bpm = bpmBox.getText();

    return "Play a " + bars + " " + style + " bass line in " + key
         + " at " + bpm + ", time signature " + timeSig
         + ". Match the current preset's sonic character.";
}

void AIAssistTab::addMessage(const juce::String& sender, const juce::String& text)
{
    auto existingText = chatDisplay.getText();
    if (existingText.isNotEmpty())
        existingText += "\n\n";

    existingText += "[" + sender + "]\n" + text;
    chatDisplay.setText(existingText);
    chatDisplay.moveCaretToEnd();
}

void AIAssistTab::setConnectionStatus(bool connected, int latMs)
{
    isConnected = connected;
    latency = latMs;

    juce::String statusText = connected
        ? "MCE: Connected (" + juce::String(latMs) + "ms)"
        : "MCE: Offline";

    statusLabel.setText(statusText, juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, connected ? BW::Green : BW::Grey);
}

// =============================================================================
// Paint / Layout
// =============================================================================

void AIAssistTab::paint(juce::Graphics& g)
{
    g.fillAll(BW::Black);

    int divX = getWidth() * 6 / 10;
    g.setColour(BW::Grey.withAlpha(0.3f));
    g.fillRect(divX, 4, 1, getHeight() - 8);
}

void AIAssistTab::resized()
{
    auto bounds = getLocalBounds().reduced(8, 4);
    int dividerX = bounds.getWidth() * 6 / 10;

    // --- Left: Chat ---
    auto chatArea = bounds.removeFromLeft(dividerX - 4);
    bounds.removeFromLeft(8);

    statusLabel.setBounds(chatArea.removeFromTop(18));

    auto inputRow = chatArea.removeFromBottom(30);
    sendBtn.setBounds(inputRow.removeFromRight(60));
    inputRow.removeFromRight(4);
    inputField.setBounds(inputRow);

    chatArea.removeFromBottom(4);
    chatDisplay.setBounds(chatArea);

    // --- Right: Context + prompts + play config ---
    auto rightPanel = bounds;

    contextTitle.setBounds(rightPanel.removeFromTop(16));
    contextDisplay.setBounds(rightPanel.removeFromTop(80).reduced(0, 2));
    rightPanel.removeFromTop(6);

    // Quick prompts
    for (int i = 0; i < 5; ++i)
        quickPrompts[i].setBounds(rightPanel.removeFromTop(26).reduced(0, 1));

    rightPanel.removeFromTop(6);

    // --- Play configuration section ---
    playConfigLabel.setBounds(rightPanel.removeFromTop(16));
    rightPanel.removeFromTop(2);

    // Row 1: Key + Style
    auto playRow1 = rightPanel.removeFromTop(24);
    int halfW = playRow1.getWidth() / 2;
    keyBox.setBounds(playRow1.removeFromLeft(halfW).reduced(1, 0));
    styleBox.setBounds(playRow1.reduced(1, 0));

    rightPanel.removeFromTop(2);

    // Row 2: Time sig + Bars
    auto playRow2 = rightPanel.removeFromTop(24);
    timeSigBox.setBounds(playRow2.removeFromLeft(halfW).reduced(1, 0));
    barsBox.setBounds(playRow2.reduced(1, 0));

    rightPanel.removeFromTop(2);

    // Row 3: BPM (full width)
    bpmBox.setBounds(rightPanel.removeFromTop(24).reduced(1, 0));

    rightPanel.removeFromTop(4);

    // Play + Stop buttons side by side
    auto playRow = rightPanel.removeFromTop(32);
    int playW = playRow.getWidth() * 2 / 3;
    playBtn.setBounds(playRow.removeFromLeft(playW).reduced(0, 2));
    stopBtn.setBounds(playRow.reduced(2, 2));

    rightPanel.removeFromTop(4);
    applyBtn.setBounds(rightPanel.removeFromTop(28).reduced(0, 2));

    // Bottom: MCE URL
    auto urlRow = rightPanel.removeFromBottom(24);
    urlLabel.setBounds(urlRow.removeFromLeft(70));
    reconnectBtn.setBounds(urlRow.removeFromRight(80));
    urlRow.removeFromRight(4);
    urlField.setBounds(urlRow);
}
