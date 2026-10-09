namespace sso
{

MidiConnection::MidiConnection()
{
    deviceListConnection = juce::MidiDeviceListConnection::make ([this] { deviceListChanged(); });
}

MidiConnection::~MidiConnection()
{
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

void MidiConnection::send (const juce::MidiMessage& m)
{
    const juce::ScopedLock sl (outputLock);

    if (output != nullptr)
    {
        output->sendMessageNow (m);
        ++numSent;
    }
}

void MidiConnection::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    ++numReceived;

    if (onMessageReceived != nullptr)
        onMessageReceived (m);
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
