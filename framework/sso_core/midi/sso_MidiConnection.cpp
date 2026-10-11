namespace sso
{

MidiConnection::MidiConnection()
{
    deviceListConnection = juce::MidiDeviceListConnection::make ([this] { deviceListChanged(); });
}

MidiConnection::~MidiConnection()
{
    *alive = false;

    {
        const juce::ScopedLock sl (inputLock);

        if (input != nullptr)
            input->stop();

        input.reset();
    }

    const juce::ScopedLock sl (outputLock);
    output.reset();
}

static bool isAvailable (const juce::Array<juce::MidiDeviceInfo>& devices, const juce::String& id)
{
    for (auto& d : devices)
        if (d.identifier == id)
            return true;

    return false;
}

bool MidiConnection::openOutput()
{
    const juce::ScopedLock sl (outputLock);
    output.reset();

    if (wantedOutput.isEmpty() || ! isAvailable (juce::MidiOutput::getAvailableDevices(), wantedOutput))
        return false;

    output = juce::MidiOutput::openDevice (wantedOutput);
    return output != nullptr;
}

bool MidiConnection::openInput()
{
    const juce::ScopedLock sl (inputLock);

    if (input != nullptr)
        input->stop();

    input.reset();

    if (wantedInput.isEmpty() || ! isAvailable (juce::MidiInput::getAvailableDevices(), wantedInput))
        return false;

    input = juce::MidiInput::openDevice (wantedInput, this);

    if (input != nullptr)
        input->start();

    return input != nullptr;
}

bool MidiConnection::setOutput (const juce::String& identifier)
{
    wantedOutput = identifier;
    auto ok = openOutput();

    if (onStateChanged != nullptr)
        onStateChanged();

    return ok || identifier.isEmpty();
}

bool MidiConnection::setInput (const juce::String& identifier)
{
    wantedInput = identifier;
    auto ok = openInput();

    if (onStateChanged != nullptr)
        onStateChanged();

    return ok || identifier.isEmpty();
}

juce::String MidiConnection::getOutputId() const  { return wantedOutput; }
juce::String MidiConnection::getInputId() const   { return wantedInput; }

bool MidiConnection::isOutputOpen() const
{
    const juce::ScopedLock sl (outputLock);
    return output != nullptr;
}

bool MidiConnection::isInputOpen() const
{
    const juce::ScopedLock sl (inputLock);
    return input != nullptr;
}

// Only short messages take part in echo detection: the synth never sends CCs, notes it
// didn't play, or requests on its own, but it does answer requests with dumps identical to
// dumps we sent earlier.
static constexpr int maxEchoCheckSize = 16;
static constexpr double echoWindowMs = 300.0;
static constexpr double loopWindowMs = 2000.0;
static constexpr int echoesForLoop = 3;

static uint64_t hashOf (const juce::MidiMessage& m)
{
    uint64_t h = 1469598103934665603ull;   // FNV-1a

    for (int i = 0; i < m.getRawDataSize(); ++i)
        h = (h ^ m.getRawData()[i]) * 1099511628211ull;

    return h;
}

void MidiConnection::rememberSent (const juce::MidiMessage& m)
{
    if (m.getRawDataSize() > maxEchoCheckSize)
        return;

    const juce::ScopedLock sl (sentLock);
    recentlySent[sentPosition] = { hashOf (m), juce::Time::getMillisecondCounterHiRes() };
    sentPosition = (sentPosition + 1) % recentlySent.size();
}

bool MidiConnection::isEchoOfSent (const juce::MidiMessage& m)
{
    if (m.getRawDataSize() > maxEchoCheckSize)
        return false;

    auto h = hashOf (m);
    auto now = juce::Time::getMillisecondCounterHiRes();
    const juce::ScopedLock sl (sentLock);

    for (auto& s : recentlySent)
        if (s.hash == h && now - s.timeMs < echoWindowMs)
            return true;

    return false;
}

void MidiConnection::clearLoopDetected()
{
    const juce::ScopedLock sl (sentLock);
    loopDetected = false;
    echoCount = 0;

    for (auto& s : recentlySent)
        s = {};
}

void MidiConnection::send (const juce::MidiMessage& m)
{
    if (loopDetected.load())
        return;     // never feed a loop

    rememberSent (m);

    if (onMessageSent != nullptr)
        onMessageSent (m);

    const juce::ScopedLock sl (outputLock);

    if (output != nullptr)
    {
        output->sendMessageNow (m);
        ++numSent;
    }
}

void MidiConnection::processIncoming (const juce::MidiMessage& m)
{
    lastHeardMs = juce::Time::getMillisecondCounterHiRes();

    // Clock, active sensing and other real-time bytes only tell us the synth is there
    if (m.getRawDataSize() == 1 && *m.getRawData() >= 0xf8)
        return;

    ++numReceived;

    if (isEchoOfSent (m))
    {
        auto now = juce::Time::getMillisecondCounterHiRes();
        bool detectedNow = false;

        {
            const juce::ScopedLock sl (sentLock);

            if (echoCount == 0 || now - firstEchoMs > loopWindowMs)
            {
                echoCount = 0;
                firstEchoMs = now;
            }

            if (++echoCount >= echoesForLoop && ! loopDetected.load())
            {
                loopDetected = true;
                detectedNow = true;
            }
        }

        if (detectedNow)
            juce::MessageManager::callAsync ([this, flag = alive]
            {
                if (*flag && onLoopDetected != nullptr)
                    onLoopDetected();
            });

        return;     // our own message - nothing for us to handle
    }

    if (onMessageReceived != nullptr)
        onMessageReceived (m);
}

void MidiConnection::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    processIncoming (m);
}

void MidiConnection::deviceListChanged()
{
    auto outputAvailable = isAvailable (juce::MidiOutput::getAvailableDevices(), wantedOutput);
    auto inputAvailable  = isAvailable (juce::MidiInput::getAvailableDevices(), wantedInput);

    if (outputAvailable != isOutputOpen())
        openOutput();

    if (inputAvailable != isInputOpen())
        openInput();

    if (onStateChanged != nullptr)
        onStateChanged();
}

} // namespace sso
