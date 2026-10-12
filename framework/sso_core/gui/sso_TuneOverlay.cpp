namespace sso
{

TuneOverlay::TuneOverlay (SynthController& c) : controller (c)
{
    levelSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    levelSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 24);
    levelSlider.setRange (SynthController::minTuneLevelDb, SynthController::maxTuneLevelDb, 0.5);
    levelSlider.setTextValueSuffix (" dB");
    levelSlider.setValue (controller.getTuneLevelDb(), juce::dontSendNotification);
    levelSlider.setDoubleClickReturnValue (true, SynthController::defaultTuneLevelDb);
    levelSlider.setTooltip ("Level of the app's reference tone - about as loud as the synth makes the beating easiest to hear");
    levelSlider.onValueChange = [this] { controller.setTuneLevelDb ((float) levelSlider.getValue()); };
    addAndMakeVisible (levelSlider);

    controller.addChangeListener (this);
    changeListenerCallback (nullptr);
}

TuneOverlay::~TuneOverlay()
{
    controller.removeChangeListener (this);
}

void TuneOverlay::setUncovered (juce::Rectangle<int> area)
{
    uncovered = area;
    resized();
    repaint();
}

juce::Rectangle<int> TuneOverlay::boxBounds() const
{
    return getLocalBounds().withTrimmedTop (uncovered.getBottom()).withSizeKeepingCentre (640, 250);
}

void TuneOverlay::resized()
{
    auto r = boxBounds().reduced (28, 18);
    levelSlider.setBounds (r.removeFromBottom (30).withTrimmedLeft (190));
}

void TuneOverlay::changeListenerCallback (juce::ChangeBroadcaster*)
{
    setVisible (controller.isTuning());
    auto nowSounding = controller.isTuneSounding();

    if (nowSounding != sounding)
    {
        sounding = nowSounding;
        repaint();
    }
}

void TuneOverlay::paint (juce::Graphics& g)
{
    {
        juce::Graphics::ScopedSaveState state (g);
        g.excludeClipRegion (uncovered);
        g.fillAll (theme::background.withAlpha (0.82f));
    }

    auto box = boxBounds();
    g.setColour (theme::panel);
    g.fillRoundedRectangle (box.toFloat(), 8.0f);
    g.setColour (theme::orange);
    g.fillRect (box.removeFromTop (6));

    auto r = box.reduced (28, 18);
    g.setColour (theme::text);
    g.setFont (theme::label (24.0f, true));
    g.drawText (sounding ? "TUNING: A = 440 HZ" : "TUNING: LOADING THE TUNE SOUND...",
                r.removeFromTop (34), juce::Justification::centredLeft);

    g.setColour (theme::textDim);
    g.setFont (theme::label (17.0f));
    g.drawFittedText ("The synth plays an A with a plain waveform, the app plays the same tone at 440 Hz. "
                      "Set the reference about as loud as the synth, then turn the synth's tune knob until "
                      "the beating slows down and stops.\nPress TUNE again to get the sound back.",
                      r.removeFromTop (110), juce::Justification::topLeft, 5);

    g.setColour (theme::text);
    g.setFont (theme::label (14.0f, true));
    g.drawText ("REFERENCE LEVEL", r.removeFromBottom (30), juce::Justification::centredLeft);
}

} // namespace sso
