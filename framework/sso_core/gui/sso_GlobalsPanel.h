#pragma once

namespace sso
{

/** The synth's global settings (tuning, arpeggiator, ...) as the app keeps them, shown in a
    call-out from the header. With "send on connect" the app sets them in the synth every
    time it starts controlling it - for synths that store globals only from their panel. */
class GlobalsPanel : public juce::Component,
                     private juce::ChangeListener
{
public:
    explicit GlobalsPanel (SynthController&);
    ~GlobalsPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override  { refresh(); }
    void refresh();

    struct Row
    {
        int index = 0;
        std::unique_ptr<juce::ComboBox> box;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ToggleButton> toggle;
    };

    SynthController& controller;
    std::vector<Row> rows;
    juce::ToggleButton sendOnConnect { "Send these settings whenever SSO connects to the synth" };
    juce::TextButton readButton { "READ FROM SYNTH" }, sendButton { "SEND NOW" };
    bool refreshing = false;

    static constexpr int rowHeight = 30, sectionHeight = 22, width = 460;
};

} // namespace sso
