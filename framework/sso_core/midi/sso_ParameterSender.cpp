namespace sso
{

// MIDI runs at 31250 baud, 10 bits per byte.
static constexpr double msPerMidiByte = 10.0 * 1000.0 / 31250.0;
static constexpr int timerIntervalMs = 2;

ParameterSender::ParameterSender (const DeviceDefinition& d, MidiConnection& c, SettingsProvider s)
    : device (d), connection (c), settings (std::move (s)),
      numParams ((int) d.getParams().size()),
      values (new std::atomic<int>[(size_t) numParams]),
      dirty (new std::atomic<bool>[(size_t) numParams])
{
    for (int i = 0; i < numParams; ++i)
    {
        values[(size_t) i] = d.getParams()[(size_t) i].defaultValue;
        dirty[(size_t) i] = false;
    }

    startTimer (timerIntervalMs);
}

ParameterSender::~ParameterSender()
{
    stopTimer();
}

void ParameterSender::setValue (int paramIndex, int value, bool transmit) noexcept
{
    if (! juce::isPositiveAndBelow (paramIndex, numParams))
        return;

    values[(size_t) paramIndex] = value;

    if (transmit)
        dirty[(size_t) paramIndex] = true;
}

int ParameterSender::getValue (int paramIndex) const noexcept
{
    return juce::isPositiveAndBelow (paramIndex, numParams) ? values[(size_t) paramIndex].load() : 0;
}

void ParameterSender::markDirty (int paramIndex) noexcept
{
    if (juce::isPositiveAndBelow (paramIndex, numParams))
        dirty[(size_t) paramIndex] = true;
}

void ParameterSender::clearDirty() noexcept
{
    for (int i = 0; i < numParams; ++i)
        dirty[(size_t) i] = false;
}

void ParameterSender::setMuted (bool shouldMute) noexcept
{
    muted = shouldMute;
}

void ParameterSender::enqueue (const juce::MidiMessage& m)
{
    if (muted.load())
        return;

    const juce::ScopedLock sl (queueLock);
    queue.push_back (m);
}

void ParameterSender::setFrozen (bool shouldFreeze) noexcept
{
    frozen = shouldFreeze;
}

bool ParameterSender::trySend (const juce::MidiMessage& m)
{
    auto now = juce::Time::getMillisecondCounterHiRes();

    if (now < lineBusyUntilMs)
        return false;

    connection.send (m);
    lineBusyUntilMs = juce::jmax (now, lineBusyUntilMs) + m.getRawDataSize() * msPerMidiByte;
    return true;
}

void ParameterSender::hiResTimerCallback()
{
    if (muted.load())
    {
        {
            const juce::ScopedLock sl (queueLock);
            queue.clear();
        }

        clearDirty();
        return;
    }

    for (;;)
    {
        juce::MidiMessage next;

        {
            const juce::ScopedLock sl (queueLock);

            if (queue.empty())
                break;

            next = queue.front();
        }

        if (! trySend (next))
            return;

        const juce::ScopedLock sl (queueLock);
        queue.pop_front();
    }

    if (frozen.load())
        return;

    auto valueOf = [this] (int i) { return values[(size_t) i].load(); };
    auto midiSettings = settings();

    for (int i = 0; i < numParams; ++i)
    {
        if (! dirty[(size_t) i].load())
            continue;

        if (juce::Time::getMillisecondCounterHiRes() < lineBusyUntilMs)
            return;

        dirty[(size_t) i] = false;

        for (auto& m : device.createParamMessages (midiSettings, i, valueOf))
        {
            connection.send (m);
            lineBusyUntilMs = juce::jmax (juce::Time::getMillisecondCounterHiRes(), lineBusyUntilMs)
                                + m.getRawDataSize() * msPerMidiByte;
        }
    }
}

} // namespace sso
