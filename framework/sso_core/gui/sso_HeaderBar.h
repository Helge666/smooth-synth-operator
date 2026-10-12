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
/** MIDI port and channel selection and AI control, shown in a call-out from the header. */
class MidiSettingsPanel : public juce::Component,
                          private juce::Timer
{
public:
    MidiSettingsPanel (SynthController&, AiControl* = nullptr);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void fill();
    void timerCallback() override;

    SynthController& controller;
    AiControl* ai;
    juce::ToggleButton aiToggle { "Let AI assistants on this computer control the synth" };
    juce::TextButton copyButton { "COPY ADDRESS" };
    juce::String aiStatus;
    juce::ComboBox outputBox, inputBox, channelBox;
    juce::TextButton resumeButton { "RESUME SENDING" };
    juce::ToggleButton forwardToggle { "Play the track's notes on the synth" };
    juce::Slider offsetSlider;
    juce::Array<juce::MidiDeviceInfo> outputs, inputs;
};

//==============================================================================
/** Logo, display, main actions and MIDI activity. */
class HeaderBar : public juce::Component,
                  public juce::TooltipClient,
                  private juce::ChangeListener,
                  private juce::Timer
{
public:
    /** `version` (e.g. "1.0.3") and `appName` (e.g. "SSO Hawk-800") show under the logo,
        with the firmware the app was made for. */
    HeaderBar (SynthController&, const juce::String& productLine, const juce::String& version = {},
               const juce::String& appName = {}, AiControl* = nullptr);
    ~HeaderBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    juce::String getTooltip() override;

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
    AiControl* ai;
    juce::String productLine, version, appName;
    double lastAiCallMs = 0;
    int aiFlash = 0;
    juce::Rectangle<int> logoArea() const;
    juce::String versionLine() const;
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
