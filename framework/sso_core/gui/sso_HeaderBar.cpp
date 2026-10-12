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

    if (conn.isLoopDetected())
        return "MIDI LOOP DETECTED - SENDING STOPPED (SEE MIDI)";

    if (! conn.isOutputOpen())
        return "NO MIDI OUTPUT - CLICK MIDI TO SET UP";

    if (controller.getPresence() == SynthController::Presence::absent)
        return "NO ANSWER FROM THE SYNTH - CHECK POWER, CABLES AND MIDI PORTS";

    if (controller.isTuning())
        return controller.isTuneSounding() ? juce::String ("TUNING: A 440 HZ - TUNE AGAIN RESTORES THE SOUND")
                                           : juce::String ("TUNING: LOADING THE TUNE SOUND...");

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
    if (ref.bank.isNotEmpty())
        right = (ref.category.isNotEmpty() ? ref.bank + " / " + ref.category : ref.bank).toUpperCase();

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
        auto problem = controller.getConnection().isLoopDetected()
                        || controller.getPresence() == SynthController::Presence::absent;
        g.setColour (problem ? theme::red : theme::lcdDim);
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

    resumeButton.setColour (juce::TextButton::buttonColourId, theme::red);
    resumeButton.onClick = [this]
    {
        controller.getConnection().clearLoopDetected();
        controller.sendChangeMessage();
        resized();
        repaint();
    };
    addChildComponent (resumeButton);

    forwardToggle.setToggleState (controller.getForwardNotes(), juce::dontSendNotification);
    forwardToggle.onClick = [this] { controller.setForwardNotes (forwardToggle.getToggleState()); };
    forwardToggle.setColour (juce::ToggleButton::textColourId, theme::text);
    forwardToggle.setColour (juce::ToggleButton::tickColourId, theme::accent);
    addAndMakeVisible (forwardToggle);

    offsetSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    offsetSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
    offsetSlider.setRange (-20.0, 100.0, 0.5);
    offsetSlider.setTextValueSuffix (" ms");
    offsetSlider.setValue (controller.getNoteOffsetMs(), juce::dontSendNotification);
    offsetSlider.setDoubleClickReturnValue (true, 0.0);
    offsetSlider.setTooltip ("Delays the notes sent to the synth so it plays in time with the other tracks");
    offsetSlider.onValueChange = [this] { controller.setNoteOffsetMs (offsetSlider.getValue()); };
    addAndMakeVisible (offsetSlider);

    fill();
    setSize (420, 430);
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

    r.removeFromTop (34);   // forward toggle
    g.setColour (theme::textDim);
    g.drawText ("NOTE TIMING OFFSET", r.removeFromTop (18), juce::Justification::bottomLeft);
    r.removeFromTop (34);   // offset slider
    r.removeFromTop (8);
    auto loop = controller.getConnection().isLoopDetected();
    g.setColour (loop ? theme::red : theme::textDim);
    g.setFont (theme::label (13.0f, loop));
    g.drawFittedText (loop ? juce::String ("MIDI loop detected: messages sent to the synth came back on the MIDI input. "
                                           "Sending has been stopped. Fix the routing, then resume.")
                           : juce::String ("Avoid MIDI loops: the synth's MIDI OUT must not be routed back to its MIDI IN "
                                           "(e.g. in Cubase remove the synth's input from 'All MIDI Inputs' and don't use "
                                           "MIDI thru for it). Loops can corrupt the synth's memory."),
                      r.removeFromTop (70), juce::Justification::topLeft, 5);
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

    forwardToggle.setBounds (r.removeFromTop (30).withTrimmedTop (4));
    r.removeFromTop (22);
    offsetSlider.setBounds (r.removeFromTop (30));

    resumeButton.setVisible (controller.getConnection().isLoopDetected());
    resumeButton.setBounds (r.removeFromBottom (32).removeFromLeft (160));
}

//==============================================================================
HeaderBar::HeaderBar (SynthController& c, const juce::String& line)
    : controller (c), productLine (line), display (c), testPlayer (c)
{
    testButton.setColour (juce::TextButton::buttonOnColourId, theme::lcdText);
    testButton.setTooltip ("Play a test phrase: four Cs, the C major scale and a chord");
    testButton.onClick = [this] { toggleTestNotes(); };
    testPlayer.onStateChanged = [this] (bool playing) { testButton.setToggleState (playing, juce::dontSendNotification); };

    tuneButton.setColour (juce::TextButton::buttonOnColourId, theme::orange);
    tuneButton.setTooltip ("Tune the synth by ear: it plays an A (440 Hz) while the same tone comes from the "
                           "app's audio output. Press again to get the sound back");
    tuneButton.onClick = [this]
    {
        testPlayer.stop();
        controller.setTuning (! controller.isTuning());
        updateTuning();
    };

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

    globalsButton.setTooltip ("The synth's global settings (tuning, arpeggiator, ...)");
    globalsButton.onClick = [this]
    {
        juce::CallOutBox::launchAsynchronously (std::make_unique<GlobalsPanel> (controller),
                                                globalsButton.getScreenBounds(), nullptr);
    };

    for (auto* b : { &browserButton, &testButton, &getButton, &sendButton, &freezeButton, &midiButton })
        addAndMakeVisible (b);

    // only for synths with a tune sound / global settings the app manages
    addChildComponent (tuneButton);
    tuneButton.setVisible (controller.canTune());
    addChildComponent (globalsButton);
    globalsButton.setVisible (! controller.getDevice().getGlobalParams().empty());

    addAndMakeVisible (display);
    controller.addChangeListener (this);
    updateTuning();
    startTimerHz (20);
}

HeaderBar::~HeaderBar()
{
    // The editor closes: no tuning without its button
    controller.removeChangeListener (this);
    controller.setTuning (false);
}

void HeaderBar::toggleTestNotes()
{
    if (controller.isTuning())
        return;

    if (testPlayer.isPlaying())
        testPlayer.stop();
    else
        testPlayer.play();
}

void HeaderBar::setBrowserVisible (bool visible)
{
    browserButton.setToggleState (visible, juce::dontSendNotification);
}

void HeaderBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    freezeButton.setToggleState (controller.isFrozen(), juce::dontSendNotification);
    updateTuning();
    repaint();
}

void HeaderBar::updateTuning()
{
    auto nowTuning = controller.isTuning();
    tuneButton.setToggleState (nowTuning, juce::dontSendNotification);

    // nothing may replace the tune sound meanwhile
    for (auto* b : { &testButton, &getButton, &sendButton, &freezeButton })
        b->setEnabled (! nowTuning);

    if (nowTuning != std::exchange (tuning, nowTuning) && onTuningChanged != nullptr)
        onTuningChanged (nowTuning);
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
        repaint (getLocalBounds().removeFromRight (62));
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

    auto leds = getLocalBounds().removeFromRight (56).withSizeKeepingCentre (56, 60);
    auto led = [&g] (juce::Rectangle<int> area, bool on, juce::Colour c, const char* text)
    {
        auto dot = area.removeFromLeft (14).withSizeKeepingCentre (9, 9).toFloat();
        g.setColour (on ? c : theme::ledOff);
        g.fillEllipse (dot);
        g.setColour (theme::textDim);
        g.setFont (theme::label (12.0f));
        g.drawText (text, area.withTrimmedLeft (4), juce::Justification::centredLeft);
    };

    auto presence = controller.getPresence();
    led (leds.removeFromTop (20), presence != SynthController::Presence::unknown,
         presence == SynthController::Presence::present ? juce::Colour (0xff4cc35a) : theme::red, "SYNTH");
    led (leds.removeFromTop (20), txFlash > 0, theme::orange, "TX");
    led (leds, rxFlash > 0, theme::lcdText, "RX");
}

void HeaderBar::resized()
{
    auto r = getLocalBounds();
    r.removeFromLeft (210);
    r.removeFromRight (62);

    std::vector<juce::TextButton*> visibleButtons;

    for (auto* b : { &browserButton, &testButton, &tuneButton, &getButton, &sendButton, &freezeButton, &midiButton, &globalsButton })
        if (b->isVisible())
            visibleButtons.push_back (b);

    constexpr int pitch = 90;
    auto buttons = r.removeFromRight ((int) visibleButtons.size() * pitch);
    for (auto* b : visibleButtons)
        b->setBounds (buttons.removeFromLeft (pitch).withSizeKeepingCentre (82, 46));

    display.setBounds (r.reduced (10, 4));
}

} // namespace sso
