#pragma once

namespace sso
{

/** Top bar: MIDI ports and channel, patch name, get/send, freeze and sync status. */
class ConnectionBar : public juce::Component,
                      private juce::ChangeListener,
                      private juce::Timer
{
public:
    explicit ConnectionBar (SynthController&);
    ~ConnectionBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refreshPorts();
    void refreshState();

    SynthController& controller;

    juce::ComboBox outputBox, inputBox, channelBox;
    juce::Label outputLabel { {}, "MIDI Out" }, inputLabel { {}, "MIDI In" }, channelLabel { {}, "Ch" };
    juce::TextEditor nameEditor;
    juce::TextButton getButton { "Get from synth" }, sendButton { "Send to synth" }, initButton { "Init" };
    juce::TextButton freezeButton { "Freeze" };
    juce::String statusText;
    juce::Colour statusColour;
    juce::Array<juce::MidiDeviceInfo> outputs, inputs;
    uint32_t lastSent = 0, lastReceived = 0;
    int txFlash = 0, rxFlash = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConnectionBar)
};

} // namespace sso
