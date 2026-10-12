namespace sso
{

TestNotePlayer::TestNotePlayer (SynthController& c) : controller (c) {}

TestNotePlayer::~TestNotePlayer()
{
    onStateChanged = nullptr;
    stop();
    *alive = false;
}

std::vector<TestNotePlayer::Event> TestNotePlayer::defaultPhrase()
{
    std::vector<Event> e;
    double t = 0;

    auto note = [&] (std::initializer_list<int> notes, double length, double gap)
    {
        for (auto n : notes) e.push_back ({ t, n, true });
        for (auto n : notes) e.push_back ({ t + length, n, false });
        t += length + gap;
    };

    for (auto n : { 36, 48, 60, 72 })                        // C2 C3 C4 C5
        note ({ n }, 450, 150);

    t += 200;

    for (auto n : { 60, 62, 64, 65, 67, 69, 71, 72 })        // C major scale C4-C5
        note ({ n }, 230, 30);

    t += 250;
    note ({ 60, 64, 67, 72 }, 1800, 0);                      // C major chord

    return e;
}

void TestNotePlayer::play (std::vector<Event> phrase)
{
    stop();

    // Silence release tails of earlier notes so the phrase starts on a quiet synth.
    controller.silence();

    {
        const juce::ScopedLock sl (lock);
        events = std::move (phrase);
        std::stable_sort (events.begin(), events.end(), [] (auto& a, auto& b) { return a.timeMs < b.timeMs; });
        next = 0;
        // after All Sound Off the synth may need a moment (the HAWK-800 a whole second)
        startMs = juce::Time::getMillisecondCounterHiRes() + juce::jmax (50, controller.getDevice().getSettleTimeMs());
        playing = true;
    }

    notifyState (true);
    startTimer (2);
}

double TestNotePlayer::getPhraseLengthMs() const
{
    const juce::ScopedLock sl (lock);
    return events.empty() ? 0.0 : events.back().timeMs;
}

void TestNotePlayer::stop()
{
    stopTimer();
    auto settings = controller.getMidiSettings();
    bool wasPlaying;

    {
        const juce::ScopedLock sl (lock);
        wasPlaying = playing.exchange (false);

        for (auto n : sounding)
            controller.sendMidi (juce::MidiMessage::noteOff (settings.channel, n));

        if (wasPlaying)
            controller.silence();

        sounding.clear();
    }

    if (wasPlaying)
        notifyState (false);
}

void TestNotePlayer::hiResTimerCallback()
{
    bool finished = false;

    {
        const juce::ScopedLock sl (lock);
        auto now = juce::Time::getMillisecondCounterHiRes() - startMs;
        auto channel = controller.getMidiSettings().channel;

        while (next < events.size() && events[next].timeMs <= now)
        {
            auto& e = events[next++];

            if (e.on)
            {
                controller.sendMidi (juce::MidiMessage::noteOn (channel, e.note, (juce::uint8) juce::jlimit (1, 127, e.velocity)));
                sounding.insert (e.note);
            }
            else
            {
                controller.sendMidi (juce::MidiMessage::noteOff (channel, e.note));
                sounding.erase (e.note);
            }
        }

        finished = next >= events.size();

        if (finished)
            playing = false;
    }

    if (finished)
    {
        stopTimer();
        notifyState (false);
    }
}

void TestNotePlayer::notifyState (bool isNowPlaying)
{
    juce::MessageManager::callAsync ([this, isNowPlaying, flag = alive]
    {
        if (*flag && onStateChanged != nullptr)
            onStateChanged (isNowPlaying);
    });
}

} // namespace sso
