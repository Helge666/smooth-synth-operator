#pragma once

namespace sso
{

/** Plays the host track's notes on the synth (a hardware synth as if it were a plugin).

    process() runs on the audio thread: it keeps notes, pitch bend, mod wheel, sustain and
    aftertouch, moves them to the synth's MIDI channel and stamps each event with the time
    it is due (its position in the block, plus the block length so events are never late,
    plus a user offset to line the synth up with the other tracks). A 1 ms timer thread
    sends them when due. Other CCs are dropped: synth parameters are automated through the
    plugin's parameters, which keeps the editor in sync.

    Held notes are tracked so they can be released (transport stop, bypass, instance
    becoming inactive or going away).
*/
class NoteForwarder : private juce::HighResolutionTimer
{
public:
    explicit NoteForwarder (SynthController&);
    ~NoteForwarder() override;

    void prepare (double sampleRate, int maximumBlockSize);

    /** Audio thread. */
    void process (const juce::MidiBuffer&, int numSamples, bool transportPlaying);

    /** Sends note-offs for all notes that are still held (any thread). */
    void releaseHeldNotes() noexcept    { releaseRequested = true; }

    static bool shouldForward (const juce::MidiMessage&);

    void setEnabled (bool shouldForward) noexcept   { enabled = shouldForward; }
    bool isEnabled() const noexcept                  { return enabled.load(); }

    /** Extra delay in ms (may be negative down to minus the block length). */
    void setUserOffsetMs (double ms) noexcept        { userOffsetMs = ms; }
    double getUserOffsetMs() const noexcept          { return userOffsetMs.load(); }
    double getBlockLengthMs() const noexcept         { return blockLengthMs.load(); }

private:
    void hiResTimerCallback() override;
    void sendNow (const juce::MidiMessage&);

    struct Event
    {
        double dueMs = 0;
        uint8_t data[3] {};
        int size = 0;
    };

    SynthController& controller;
    juce::AbstractFifo fifo { 2048 };
    std::array<Event, 2048> events;

    std::atomic<double> sampleRate { 48000.0 }, blockLengthMs { 10.0 }, userOffsetMs { 0.0 };
    std::atomic<bool> enabled { true }, releaseRequested { false };
    bool wasPlaying = false, wasActive = false;     // audio thread

    // timer thread
    std::bitset<128> held;
    bool sustainHeld = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoteForwarder)
};

} // namespace sso
