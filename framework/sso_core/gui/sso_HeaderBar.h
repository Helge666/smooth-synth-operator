#pragma once

namespace sso
{

/** Green two-line display: current sound, and the parameter being edited / status. */
class LcdDisplay : public juce::Component,
                   public juce::SettableTooltipClient,
                   private juce::ChangeListener,
                   private juce::Timer
{
public:
    explicit LcdDisplay (SynthController&);
    ~LcdDisplay() override;

    void paint (juce::Graphics&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    std::function<void()> onRenameRequested;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    void timerCallback() override;
    juce::String statusLine() const;

    SynthController& controller;
    uint32_t lastChangeCount = 0;
    int touchedParam = -1;
    int touchedValue = 0;
    double touchedUntil = 0;
};

//==============================================================================
/** MIDI port and channel selection, shown in a call-out from the header. */
class MidiSettingsPanel : public juce::Component
{
public:
    explicit MidiSettingsPanel (SynthController&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void fill();

    SynthController& controller;
    juce::ComboBox outputBox, inputBox, channelBox;
    juce::TextButton resumeButton { "RESUME SENDING" };
    juce::ToggleButton forwardToggle { "Play the track's notes on the synth" };
    juce::Slider offsetSlider;
    juce::Array<juce::MidiDeviceInfo> outputs, inputs;
};

//==============================================================================
/** Logo, display, main actions and MIDI activity. */
class HeaderBar : public juce::Component,
                  private juce::ChangeListener,
                  private juce::Timer
{
public:
    HeaderBar (SynthController&, const juce::String& productLine);
    ~HeaderBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (bool)> onBrowserToggled;
    void setBrowserVisible (bool);

    LcdDisplay& getDisplay() noexcept { return display; }

    /** Plays/stops the test phrase (TEST button, space key in the standalone app). */
    void toggleTestNotes();

    /** Called when tuning (TUNE) starts or ends. */
    std::function<void (bool)> onTuningChanged;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    SynthController& controller;
    juce::String productLine;
    LcdDisplay display;
    void updateTuning();

    juce::TextButton browserButton { "BROWSER" }, testButton { "TEST" }, tuneButton { "TUNE" }, getButton { "GET" }, sendButton { "SEND" },
                     freezeButton { "FREEZE" }, midiButton { "MIDI" }, globalsButton { "GLOBALS" };
    TestNotePlayer testPlayer;
    bool tuning = false;
    uint32_t lastSent = 0, lastReceived = 0;
    int txFlash = 0, rxFlash = 0;
};

} // namespace sso
