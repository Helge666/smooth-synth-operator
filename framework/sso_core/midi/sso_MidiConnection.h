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

    //==============================================================================
    /** MIDI loop protection: if messages we sent come back on our input, something routes
        our output back to us (MIDI thru, a merger, a DAW track with thru). Then sending is
        stopped until clearLoopDetected() - a loop can corrupt the synth's memory. */
    bool isLoopDetected() const noexcept      { return loopDetected.load(); }
    void clearLoopDetected();

    /** Called on the message thread when a loop is detected. */
    std::function<void()> onLoopDetected;

    /** Called (on the sending thread) for every message passed to send() that is not blocked
        by the loop protection - also when no port is open. For monitoring and tests. */
    std::function<void (const juce::MidiMessage&)> onMessageSent;

    /** Feeds a message as if it had arrived on the input (used by the MIDI callback and tests). */
    void processIncoming (const juce::MidiMessage&);

private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void deviceListChanged();
    void rememberSent (const juce::MidiMessage&);
    bool isEchoOfSent (const juce::MidiMessage&);
    bool openOutput();
    bool openInput();

    mutable juce::CriticalSection outputLock, inputLock;
    std::unique_ptr<juce::MidiOutput> output;
    std::unique_ptr<juce::MidiInput> input;
    juce::String wantedOutput, wantedInput;
    std::atomic<uint32_t> numSent { 0 }, numReceived { 0 };

    struct SentMessage { uint64_t hash = 0; double timeMs = -1.0e9; };
    std::array<SentMessage, 64> recentlySent;
    size_t sentPosition = 0;
    juce::CriticalSection sentLock;
    int echoCount = 0;
    double firstEchoMs = 0;
    std::atomic<bool> loopDetected { false };
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
    juce::MidiDeviceListConnection deviceListConnection;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiConnection)
};

} // namespace sso
