namespace sso
{

NoteForwarder::NoteForwarder (SynthController& c) : controller (c)
{
    startTimer (1);
}

NoteForwarder::~NoteForwarder()
{
    stopTimer();
    releaseRequested = true;
    hiResTimerCallback();       // flush: nothing may keep sounding
}

void NoteForwarder::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate > 0 ? newSampleRate : 48000.0;
    blockLengthMs = 1000.0 * juce::jmax (1, maximumBlockSize) / sampleRate.load();
}

bool NoteForwarder::shouldForward (const juce::MidiMessage& m)
{
    if (m.isNoteOnOrOff() || m.isPitchWheel() || m.isChannelPressure() || m.isAftertouch())
        return true;

    // mod wheel and sustain pedal; other CCs would change synth parameters behind the editor's back
    return m.isController() && (m.getControllerNumber() == 1 || m.getControllerNumber() == 64);
}

void NoteForwarder::process (const juce::MidiBuffer& midi, int numSamples, bool transportPlaying)
{
    juce::ignoreUnused (numSamples);
    auto active = controller.isActive() && enabled.load();

    // Release what is held when playback stops or this instance stops forwarding
    if ((wasPlaying && ! transportPlaying) || (wasActive && ! active))
        releaseRequested = true;

    wasPlaying = transportPlaying;
    wasActive = active;

    if (! active || midi.isEmpty())
        return;

    auto blockStart = juce::Time::getMillisecondCounterHiRes();
    auto delay = juce::jmax (0.0, blockLengthMs.load() + userOffsetMs.load());
    auto channel = controller.getMidiSettings().channel;
    auto rate = sampleRate.load();

    for (const auto metadata : midi)
    {
        auto message = metadata.getMessage();

        if (! shouldForward (message) || message.getRawDataSize() > 3)
            continue;

        message.setChannel (channel);

        auto scope = fifo.write (1);

        if (scope.blockSize1 + scope.blockSize2 == 0)
            break;      // full - a burst of thousands of events per block; drop the rest

        auto& e = events[(size_t) (scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2)];
        e.dueMs = blockStart + delay + 1000.0 * metadata.samplePosition / rate;
        e.size = message.getRawDataSize();
        std::memcpy (e.data, message.getRawData(), (size_t) e.size);
    }
}

void NoteForwarder::sendNow (const juce::MidiMessage& m)
{
    // Direct to the output (not through the paced parameter queue): notes must not wait
    // behind a sound dump. MidiConnection::send is thread safe and respects loop protection.
    if (m.isNoteOn())
        held.set ((size_t) m.getNoteNumber());
    else if (m.isNoteOff())
        held.reset ((size_t) m.getNoteNumber());
    else if (m.isSustainPedalOn())
        sustainHeld = true;
    else if (m.isSustainPedalOff())
        sustainHeld = false;

    controller.getConnection().send (m);
}

void NoteForwarder::hiResTimerCallback()
{
    auto now = juce::Time::getMillisecondCounterHiRes();

    for (;;)
    {
        // look at the oldest event first; only consume it when it is due
        int start1, size1, start2, size2;
        fifo.prepareToRead (1, start1, size1, start2, size2);

        if (size1 + size2 == 0)
            break;

        auto& e = events[(size_t) (size1 > 0 ? start1 : start2)];

        if (e.dueMs > now && ! releaseRequested.load())
            break;

        juce::MidiMessage message (e.data, e.size);
        fifo.finishedRead (1);
        sendNow (message);
    }

    if (releaseRequested.exchange (false))
    {
        auto channel = controller.getMidiSettings().channel;

        if (sustainHeld)
            sendNow (juce::MidiMessage::controllerEvent (channel, 64, 0));

        for (int n = 0; n < 128; ++n)
            if (held[(size_t) n])
                sendNow (juce::MidiMessage::noteOff (channel, n));
    }
}

} // namespace sso
