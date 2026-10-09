namespace sso
{

LcdDisplay::LcdDisplay (SynthController& c) : controller (c)
{
    controller.addChangeListener (this);
    lastChangeCount = controller.getUserChangeCount();
    setTooltip ("Double-click to rename the sound");
    startTimerHz (25);
}

LcdDisplay::~LcdDisplay()
{
    controller.removeChangeListener (this);
}

void LcdDisplay::timerCallback()
{
    auto count = controller.getUserChangeCount();
    auto now = juce::Time::getMillisecondCounterHiRes();

    if (count != lastChangeCount)
    {
        lastChangeCount = count;
        touchedParam = controller.getLastUserChange();
        touchedUntil = now + 2000.0;
    }

    if (touchedParam >= 0)
    {
        auto value = controller.getValue (touchedParam);

        if (value != touchedValue || now > touchedUntil)
        {
            touchedValue = value;

            if (now > touchedUntil)
                touchedParam = -1;

            repaint();
        }
    }
}

juce::String LcdDisplay::statusLine() const
{
    auto& conn = controller.getConnection();

    if (! conn.isOutputOpen())
        return "NO MIDI OUTPUT - CLICK MIDI TO SET UP";

    juce::String sync;

    switch (controller.getSyncState())
    {
        case SynthController::SyncState::unknown:          sync = "NOT SYNCED"; break;
        case SynthController::SyncState::waitingForSynth:  sync = "WAITING FOR SYNTH"; break;
        case SynthController::SyncState::inSync:           sync = "IN SYNC"; break;
        case SynthController::SyncState::edited:           sync = "EDITED"; break;
    }

    return sync + "  CH " + juce::String (controller.getMidiSettings().channel)
           + (conn.isInputOpen() ? juce::String() : juce::String ("  NO MIDI IN"))
           + (controller.isFrozen() ? "  FREEZE" : "");
}

void LcdDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::lcdBackground);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (theme::lcdBorder);
    g.drawRoundedRectangle (r.reduced (1.0f), 6.0f, 2.0f);

    auto inner = getLocalBounds().reduced (16, 8);
    auto top = inner.removeFromTop (inner.getHeight() * 3 / 5);
    auto& ref = controller.getPresetRef();

    g.setFont (theme::lcd (30.0f));
    g.setColour (theme::lcdText);
    auto name = controller.getPatchName().toUpperCase();
    g.drawText (name.isEmpty() ? juce::String ("(NO NAME)") : name, top, juce::Justification::centredLeft);

    juce::String right;
    if (ref.file != juce::File())
        right = (ref.bank + " / " + ref.category).toUpperCase();

    g.setFont (theme::lcd (18.0f));
    g.setColour (theme::lcdDim);
    g.drawText (right, top, juce::Justification::centredRight);

    if (touchedParam >= 0)
    {
        auto& spec = controller.getDevice().getParams()[(size_t) touchedParam];
        g.setColour (theme::lcdText);
        g.drawText ((spec.section + " " + spec.name + "  " + spec.toText (touchedValue)).toUpperCase(), inner,
                    juce::Justification::centredLeft);
    }
    else
    {
        g.drawText (statusLine(), inner, juce::Justification::centredLeft);
    }
}

void LcdDisplay::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onRenameRequested != nullptr)
        onRenameRequested();
}

//==============================================================================
MidiSettingsPanel::MidiSettingsPanel (SynthController& c) : controller (c)
{
    for (int ch = 1; ch <= 16; ++ch)
        channelBox.addItem ("Channel " + juce::String (ch), ch);

    channelBox.setSelectedId (controller.getMidiSettings().channel, juce::dontSendNotification);
    channelBox.onChange = [this] { controller.setMidiChannel (channelBox.getSelectedId()); };

    outputBox.onChange = [this]
    {
        auto i = outputBox.getSelectedId() - 2;
        controller.setMidiOutput (juce::isPositiveAndBelow (i, outputs.size()) ? outputs[i].identifier : juce::String());
    };

    inputBox.onChange = [this]
    {
        auto i = inputBox.getSelectedId() - 2;
        controller.setMidiInput (juce::isPositiveAndBelow (i, inputs.size()) ? inputs[i].identifier : juce::String());
    };

    for (auto* box : { &outputBox, &inputBox, &channelBox })
        addAndMakeVisible (box);

    fill();
    setSize (380, 170);
}

void MidiSettingsPanel::fill()
{
    auto populate = [] (juce::ComboBox& box, const juce::Array<juce::MidiDeviceInfo>& devices, const juce::String& current)
    {
        box.clear (juce::dontSendNotification);
        box.addItem ("<none>", 1);
        int selected = 1;

        for (int i = 0; i < devices.size(); ++i)
        {
            box.addItem (devices[i].name, i + 2);

            if (devices[i].identifier == current)
                selected = i + 2;
        }

        if (current.isNotEmpty() && selected == 1)
        {
            box.addItem ("(unavailable) " + current, 1000);
            selected = 1000;
        }

        box.setSelectedId (selected, juce::dontSendNotification);
    };

    outputs = juce::MidiOutput::getAvailableDevices();
    inputs = juce::MidiInput::getAvailableDevices();
    populate (outputBox, outputs, controller.getConnection().getOutputId());
    populate (inputBox, inputs, controller.getConnection().getInputId());
}

void MidiSettingsPanel::paint (juce::Graphics& g)
{
    g.setColour (theme::textDim);
    g.setFont (theme::label (13.0f));
    auto r = getLocalBounds().reduced (12);

    for (auto* text : { "TO SYNTH (MIDI OUT)", "FROM SYNTH (MIDI IN)", "MIDI CHANNEL" })
    {
        g.drawText (text, r.removeFromTop (18), juce::Justification::bottomLeft);
        r.removeFromTop (32);
    }
}

void MidiSettingsPanel::resized()
{
    auto r = getLocalBounds().reduced (12);

    for (auto* box : { &outputBox, &inputBox, &channelBox })
    {
        r.removeFromTop (18);
        box->setBounds (r.removeFromTop (28));
        r.removeFromTop (4);
    }
}

//==============================================================================
HeaderBar::HeaderBar (SynthController& c, const juce::String& line)
    : controller (c), productLine (line), display (c)
{
    browserButton.setClickingTogglesState (true);
    browserButton.onClick = [this]
    {
        if (onBrowserToggled != nullptr)
            onBrowserToggled (browserButton.getToggleState());
    };

    getButton.setTooltip ("Get the current sound from the synth");
    getButton.onClick = [this] { controller.requestFromSynth(); };
    sendButton.setTooltip ("Send the sound to the synth's edit buffer");
    sendButton.onClick = [this] { controller.sendToSynth(); };

    freezeButton.setClickingTogglesState (true);
    freezeButton.setColour (juce::TextButton::buttonOnColourId, theme::red);
    freezeButton.setTooltip ("Hold back edits; they are sent when released");
    freezeButton.onClick = [this] { controller.setFrozen (freezeButton.getToggleState()); };

    midiButton.setTooltip ("MIDI ports and channel");
    midiButton.onClick = [this]
    {
        juce::CallOutBox::launchAsynchronously (std::make_unique<MidiSettingsPanel> (controller),
                                                midiButton.getScreenBounds(), nullptr);
    };

    for (auto* b : { &browserButton, &getButton, &sendButton, &freezeButton, &midiButton })
        addAndMakeVisible (b);

    addAndMakeVisible (display);
    controller.addChangeListener (this);
    startTimerHz (20);
}

HeaderBar::~HeaderBar()
{
    controller.removeChangeListener (this);
}

void HeaderBar::setBrowserVisible (bool visible)
{
    browserButton.setToggleState (visible, juce::dontSendNotification);
}

void HeaderBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    freezeButton.setToggleState (controller.isFrozen(), juce::dontSendNotification);
    repaint();
}

void HeaderBar::timerCallback()
{
    auto& conn = controller.getConnection();
    auto sent = conn.getNumSent(), received = conn.getNumReceived();

    if (sent != lastSent)         { lastSent = sent; txFlash = 3; }
    if (received != lastReceived) { lastReceived = received; rxFlash = 3; }

    if (txFlash > 0 || rxFlash > 0)
    {
        txFlash = juce::jmax (0, txFlash - 1);
        rxFlash = juce::jmax (0, rxFlash - 1);
        repaint (getLocalBounds().removeFromRight (60));
    }
}

void HeaderBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    auto logo = r.removeFromLeft (200).withSizeKeepingCentre (200, 60);

    g.setColour (theme::text);
    g.setFont (theme::label (40.0f, true));
    g.drawText ("SSO", logo.removeFromTop (38), juce::Justification::bottomLeft);
    g.setColour (theme::textDim);
    g.setFont (theme::label (13.0f).withExtraKerningFactor (0.2f));
    g.drawText (productLine.toUpperCase(), logo, juce::Justification::topLeft);

    auto leds = getLocalBounds().removeFromRight (50).withSizeKeepingCentre (50, 44);
    auto led = [&g] (juce::Rectangle<int> area, bool on, juce::Colour c, const char* text)
    {
        auto dot = area.removeFromLeft (14).withSizeKeepingCentre (9, 9).toFloat();
        g.setColour (on ? c : theme::ledOff);
        g.fillEllipse (dot);
        g.setColour (theme::textDim);
        g.setFont (theme::label (12.0f));
        g.drawText (text, area.withTrimmedLeft (4), juce::Justification::centredLeft);
    };

    led (leds.removeFromTop (22), txFlash > 0, theme::orange, "TX");
    led (leds, rxFlash > 0, theme::lcdText, "RX");
}

void HeaderBar::resized()
{
    auto r = getLocalBounds();
    r.removeFromLeft (210);
    r.removeFromRight (56);

    auto buttons = r.removeFromRight (5 * 96);
    for (auto* b : { &browserButton, &getButton, &sendButton, &freezeButton, &midiButton })
        b->setBounds (buttons.removeFromLeft (96).reduced (4, 0).withSizeKeepingCentre (88, 46));

    display.setBounds (r.reduced (10, 4));
}

} // namespace sso
