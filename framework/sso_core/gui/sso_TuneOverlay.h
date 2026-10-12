#pragma once

namespace sso
{

/** Covers browser and panel while tuning (TUNE), so nothing changes the synth's sound
    meanwhile. The header stays usable - its TUNE button ends tuning. */
class TuneOverlay : public juce::Component,
                    private juce::ChangeListener
{
public:
    explicit TuneOverlay (SynthController&);
    ~TuneOverlay() override;

    /** The area left uncovered (the header), in this component's coordinates. */
    void setUncovered (juce::Rectangle<int>);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool hitTest (int x, int y) override     { return ! uncovered.contains (x, y); }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    juce::Rectangle<int> boxBounds() const;

    SynthController& controller;
    juce::Slider levelSlider;
    juce::Rectangle<int> uncovered;
    bool sounding = false;
};

} // namespace sso
