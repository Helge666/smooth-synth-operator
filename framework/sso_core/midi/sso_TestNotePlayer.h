#pragma once

#include <set>

namespace sso
{

/** Plays a short test phrase on the synth, so a change can be heard without walking
    to the keyboard. Default phrase: four Cs from low to high, the C major scale in
    the middle, then a C major chord. */
class TestNotePlayer : private juce::HighResolutionTimer
{
public:
    struct Event
    {
        double timeMs;
        int note;
        bool on;
    };

    explicit TestNotePlayer (SynthController&);
    ~TestNotePlayer() override;

    void play (std::vector<Event> phrase = defaultPhrase());
    void stop();
    bool isPlaying() const noexcept { return playing.load(); }

    /** Called on the message thread when playing starts or stops. */
    std::function<void (bool)> onStateChanged;

    static std::vector<Event> defaultPhrase();

private:
    void hiResTimerCallback() override;
    void notifyState (bool);

    SynthController& controller;
    std::vector<Event> events;
    size_t next = 0;
    double startMs = 0;
    std::atomic<bool> playing { false };
    std::set<int> sounding;
    juce::CriticalSection lock;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);   // guards async callbacks
};

} // namespace sso
