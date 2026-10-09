#pragma once

namespace sso
{

/** Reads a list of stored sounds from the synth, one request at a time.

    Requesting a stored sound replaces the synth's edit buffer, so when finished the
    sound currently open in the editor is sent back to the synth.
*/
class BankReader : private juce::Timer
{
public:
    explicit BankReader (SynthController&);
    ~BankReader() override;

    using Result = std::vector<DeviceDefinition::IncomingPatch>;

    /** onProgress(done, total) and onFinished(result, complete) are called on the message thread. */
    void start (std::vector<int> slots,
                std::function<void (int, int)> onProgress,
                std::function<void (Result, bool complete)> onFinished,
                bool restoreEditBuffer = true);

    void cancel();
    bool isRunning() const noexcept { return running; }

private:
    void timerCallback() override;
    void requestCurrent();
    void received (const DeviceDefinition::IncomingPatch&);
    void finish (bool complete);

    SynthController& controller;
    std::vector<int> slots;
    size_t current = 0;
    int attempts = 0;
    bool running = false;
    bool restoreAfterwards = true;
    Result result;
    std::function<void (int, int)> progressCallback;
    std::function<void (Result, bool)> finishedCallback;

    static constexpr int timeoutMs = 1000;
    static constexpr int maxAttempts = 3;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BankReader)
};

} // namespace sso
