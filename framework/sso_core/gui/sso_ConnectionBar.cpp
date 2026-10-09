namespace sso
{

ConnectionBar::ConnectionBar (SynthController& c) : controller (c)
{
    for (auto* l : { &outputLabel, &inputLabel, &channelLabel })
    {
        l->setFont (juce::FontOptions (12.0f));
        l->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (*l);
    }

    for (int ch = 1; ch <= 16; ++ch)
        channelBox.addItem (juce::String (ch), ch);

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

    nameEditor.setInputRestrictions (controller.getDevice().getMaxNameLength());
    nameEditor.setJustification (juce::Justification::centredLeft);
    nameEditor.setFont (juce::FontOptions (16.0f));
    nameEditor.onReturnKey = nameEditor.onFocusLost = [this] { controller.setPatchName (nameEditor.getText()); };

    getButton.onClick  = [this] { controller.requestFromSynth(); };
    sendButton.onClick = [this] { controller.sendToSynth(); };
    initButton.onClick = [this] { controller.loadPatch (controller.getDevice().createInitPatch(), true); };

    freezeButton.setClickingTogglesState (true);
    freezeButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd8402f));
    freezeButton.onClick = [this] { controller.setFrozen (freezeButton.getToggleState()); };

    for (auto* comp : std::initializer_list<juce::Component*> { &outputBox, &inputBox, &channelBox, &nameEditor,
                                                                &getButton, &sendButton, &initButton, &freezeButton })
        addAndMakeVisible (comp);

    refreshPorts();
    refreshState();
    controller.addChangeListener (this);
    startTimerHz (20);
}

ConnectionBar::~ConnectionBar()
{
    controller.removeChangeListener (this);
}

void ConnectionBar::refreshPorts()
{
    auto fill = [] (juce::ComboBox& box, const juce::Array<juce::MidiDeviceInfo>& devices, const juce::String& current, bool open)
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
        box.setColour (juce::ComboBox::outlineColourId, open || current.isEmpty() ? juce::Colours::transparentBlack
                                                                                  : juce::Colours::red);
    };

    auto& conn = controller.getConnection();
    outputs = juce::MidiOutput::getAvailableDevices();
    inputs = juce::MidiInput::getAvailableDevices();
    fill (outputBox, outputs, conn.getOutputId(), conn.isOutputOpen());
    fill (inputBox, inputs, conn.getInputId(), conn.isInputOpen());
}

void ConnectionBar::refreshState()
{
    if (! nameEditor.hasKeyboardFocus (true))
        nameEditor.setText (controller.getPatchName().trimEnd(), juce::dontSendNotification);

    freezeButton.setToggleState (controller.isFrozen(), juce::dontSendNotification);

    auto& conn = controller.getConnection();

    if (! conn.isOutputOpen())
    {
        statusText = "No MIDI output";
        statusColour = juce::Colours::red;
    }
    else
    {
        switch (controller.getSyncState())
        {
            case SynthController::SyncState::unknown:          statusText = "Not synced";          statusColour = juce::Colours::grey; break;
            case SynthController::SyncState::waitingForSynth:  statusText = "Waiting for synth..."; statusColour = juce::Colours::orange; break;
            case SynthController::SyncState::inSync:           statusText = "In sync";              statusColour = juce::Colour (0xff4caf50); break;
            case SynthController::SyncState::edited:           statusText = "Edited";               statusColour = juce::Colour (0xfff0b030); break;
        }
    }

    repaint();
}

void ConnectionBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshPorts();
    refreshState();
}

void ConnectionBar::timerCallback()
{
    auto& conn = controller.getConnection();
    auto sent = conn.getNumSent(), received = conn.getNumReceived();

    if (sent != lastSent)         { lastSent = sent; txFlash = 3; }
    if (received != lastReceived) { lastReceived = received; rxFlash = 3; }

    if (txFlash > 0 || rxFlash > 0)
    {
        txFlash = juce::jmax (0, txFlash - 1);
        rxFlash = juce::jmax (0, rxFlash - 1);
        repaint();
    }

    if (controller.getSyncState() == SynthController::SyncState::waitingForSynth)
        repaint();
}

void ConnectionBar::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e21));

    auto r = getLocalBounds().removeFromRight (230).reduced (8, 0);
    auto led = [&] (juce::Rectangle<int> area, bool on, juce::Colour c, const char* text)
    {
        auto dot = area.removeFromLeft (14).withSizeKeepingCentre (10, 10).toFloat();
        g.setColour (on ? c : c.withAlpha (0.2f));
        g.fillEllipse (dot);
        g.setColour (juce::Colours::lightgrey);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (text, area.withTrimmedLeft (3), juce::Justification::centredLeft);
    };

    auto ledArea = r.removeFromLeft (70);
    led (ledArea.removeFromTop (ledArea.getHeight() / 2), txFlash > 0, juce::Colour (0xffff5030), "TX");
    led (ledArea, rxFlash > 0, juce::Colour (0xff40d060), "RX");

    g.setColour (statusColour);
    g.fillRoundedRectangle (r.withSizeKeepingCentre (r.getWidth(), 26).toFloat(), 4.0f);
    g.setColour (juce::Colours::black);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (statusText, r, juce::Justification::centred);
}

void ConnectionBar::resized()
{
    auto r = getLocalBounds().reduced (6);
    r.removeFromRight (230);

    auto place = [&r] (juce::Component& c, int w) { c.setBounds (r.removeFromLeft (w).reduced (2, 4)); };

    place (outputLabel, 62);  place (outputBox, 200);
    place (inputLabel, 56);   place (inputBox, 200);
    place (channelLabel, 26); place (channelBox, 56);
    r.removeFromLeft (12);
    place (nameEditor, 200);
    place (getButton, 116);
    place (sendButton, 110);
    place (initButton, 50);
    place (freezeButton, 70);
}

} // namespace sso
