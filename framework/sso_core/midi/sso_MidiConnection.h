#pragma once

namespace sso
{

/** One bidirectional MIDI link to a hardware synth.

    Remembers the wanted ports by identifier and reconnects automatically when the
    device list changes (e.g. USB interface replugged). Sending is thread safe.
*/
class MidiConnection : private juce::MidiInputCallback
{
public:
    MidiConnection();
    ~MidiConnection() override;

    /** Empty identifier closes the port. Returns false if the port could not be opened. */
    bool setOutput (const juce::String& identifier);
    bool setInput (const juce::String& identifier);

    juce::String getOutputId() const;
    juce::String getInputId() const;
    bool isOutputOpen() const;
    bool isInputOpen() const;

    void send (const juce::MidiMessage&);

    /** Called on the MIDI thread for every incoming message. Set before opening ports. */
    std::function<void (const juce::MidiMessage&)> onMessageReceived;

    /** Called on the message thread when ports appear/disappear or open/close. */
    std::function<void()> onStateChanged;

    uint32_t getNumSent() const noexcept      { return numSent.load(); }
    uint32_t getNumReceived() const noexcept  { return numReceived.load(); }

private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void deviceListChanged();
    bool openOutput();
    bool openInput();

    mutable juce::CriticalSection outputLock, inputLock;
    std::unique_ptr<juce::MidiOutput> output;
    std::unique_ptr<juce::MidiInput> input;
    juce::String wantedOutput, wantedInput;
    std::atomic<uint32_t> numSent { 0 }, numReceived { 0 };
    juce::MidiDeviceListConnection deviceListConnection;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiConnection)
};

} // namespace sso
