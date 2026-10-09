#pragma once

namespace sso
{

/** Greys out an editor whose instance does not control the synth, with a hint who does
    and a TAKE OVER button. Hidden while the instance is active. */
class InactiveOverlay : public juce::Component,
                        private juce::ChangeListener,
                        private juce::Timer
{
public:
    explicit InactiveOverlay (SynthController&);
    ~InactiveOverlay() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override  { update(); }
    void timerCallback() override                                     { update(); }
    void update();

    SynthController& controller;
    juce::TextButton takeOverButton { "TAKE OVER" };
    juce::String ownerText;
};

} // namespace sso
