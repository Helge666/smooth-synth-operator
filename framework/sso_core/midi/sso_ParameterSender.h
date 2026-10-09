#pragma once

namespace sso
{

/** Transmits parameter changes and bulk messages to the synth from a timer thread.

    - Parameter values can be set from any thread (GUI, host automation, audio thread).
      Only the latest value of each parameter is sent ("coalescing"), which keeps fast
      knob sweeps from flooding the 31.25 kBaud MIDI line.
    - Output is paced to the MIDI wire speed.
    - While frozen, edits accumulate and are sent when the freeze is released.
*/
class ParameterSender : private juce::HighResolutionTimer
{
public:
    using SettingsProvider = std::function<DeviceDefinition::MidiSettings()>;

    ParameterSender (const DeviceDefinition&, MidiConnection&, SettingsProvider);
    ~ParameterSender() override;

    /** Stores the value. If `transmit` is true it will be sent to the synth. */
    void setValue (int paramIndex, int value, bool transmit) noexcept;
    int getValue (int paramIndex) const noexcept;
    void markDirty (int paramIndex) noexcept;
    void clearDirty() noexcept;

    /** Queues a complete message (dump, request...). Queued messages go out before
        pending parameter changes, in order. */
    void enqueue (const juce::MidiMessage&);

    void setFrozen (bool) noexcept;

    /** While muted nothing is sent; pending changes and queued messages are dropped. */
    void setMuted (bool) noexcept;
    bool isFrozen() const noexcept { return frozen.load(); }

private:
    void hiResTimerCallback() override;
    bool trySend (const juce::MidiMessage&);

    const DeviceDefinition& device;
    MidiConnection& connection;
    SettingsProvider settings;

    const int numParams;
    std::unique_ptr<std::atomic<int>[]> values;
    std::unique_ptr<std::atomic<bool>[]> dirty;
    std::atomic<bool> frozen { false }, muted { false };

    juce::CriticalSection queueLock;
    std::deque<juce::MidiMessage> queue;

    double lineBusyUntilMs = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterSender)
};

} // namespace sso
