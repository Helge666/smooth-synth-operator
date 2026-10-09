namespace sso
{

InactiveOverlay::InactiveOverlay (SynthController& c) : controller (c)
{
    takeOverButton.setColour (juce::TextButton::buttonColourId, theme::yellow);
    takeOverButton.setColour (juce::TextButton::textColourOffId, theme::background);
    takeOverButton.onClick = [this] { controller.takeOver(); update(); };
    addAndMakeVisible (takeOverButton);

    controller.addChangeListener (this);
    startTimer (500);   // the owner's description can change in another process
    update();
}

InactiveOverlay::~InactiveOverlay()
{
    controller.removeChangeListener (this);
}

void InactiveOverlay::update()
{
    setVisible (! controller.isActive());

    if (! isVisible())
        return;

    auto owner = controller.getOwnerDescription();
    auto text = controller.isTakingOver()
                    ? juce::String ("Taking over...")
                    : (owner.isNotEmpty() ? "The synth is controlled by " + owner + "."
                                          : juce::String ("No SSO instance controls the synth at the moment."));

    takeOverButton.setEnabled (! controller.isTakingOver());

    if (text != ownerText)
    {
        ownerText = text;
        repaint();
    }
}

void InactiveOverlay::paint (juce::Graphics& g)
{
    g.fillAll (theme::background.withAlpha (0.82f));

    auto box = getLocalBounds().withSizeKeepingCentre (640, 220);
    g.setColour (theme::panel);
    g.fillRoundedRectangle (box.toFloat(), 8.0f);
    g.setColour (theme::yellow);
    g.fillRect (box.removeFromTop (6));

    auto r = box.reduced (28, 18);
    g.setColour (theme::text);
    g.setFont (theme::label (24.0f, true));
    g.drawText ("THIS INSTANCE IS INACTIVE", r.removeFromTop (34), juce::Justification::centredLeft);

    g.setColour (theme::textDim);
    g.setFont (theme::label (17.0f));
    g.drawFittedText (ownerText + "\nOnly one SSO instance can control a synth at a time. Taking over sends "
                                  "this instance's sound to the synth.",
                      r.removeFromTop (80), juce::Justification::topLeft, 3);
}

void InactiveOverlay::resized()
{
    auto box = getLocalBounds().withSizeKeepingCentre (640, 220);
    takeOverButton.setBounds (box.removeFromBottom (68).reduced (28, 14).removeFromLeft (180));
}

} // namespace sso
