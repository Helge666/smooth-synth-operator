namespace sso
{

BankReader::BankReader (SynthController& c) : controller (c) {}

BankReader::~BankReader()
{
    if (running)
        controller.onSlotReceived = nullptr;
}

void BankReader::start (std::vector<int> slotsToRead,
                        std::function<void (int, int)> onProgress,
                        std::function<void (Result, bool)> onFinished)
{
    jassert (! running);
    slots = std::move (slotsToRead);
    progressCallback = std::move (onProgress);
    finishedCallback = std::move (onFinished);
    result.clear();
    current = 0;
    running = true;

    controller.onSlotReceived = [this] (const DeviceDefinition::IncomingPatch& p) { received (p); };
    requestCurrent();
}

void BankReader::cancel()
{
    if (running)
        finish (false);
}

void BankReader::requestCurrent()
{
    if (current >= slots.size())
    {
        finish (true);
        return;
    }

    attempts = 1;
    controller.requestSlot (slots[current]);
    startTimer (timeoutMs);
}

void BankReader::received (const DeviceDefinition::IncomingPatch& p)
{
    if (! running || current >= slots.size() || p.slot != slots[current])
        return;

    stopTimer();
    result.push_back (p);
    ++current;

    if (progressCallback != nullptr)
        progressCallback ((int) current, (int) slots.size());

    requestCurrent();
}

void BankReader::timerCallback()
{
    if (attempts < maxAttempts)
    {
        ++attempts;
        controller.requestSlot (slots[current]);
        return;
    }

    finish (false);
}

void BankReader::finish (bool complete)
{
    stopTimer();
    running = false;
    controller.onSlotReceived = nullptr;

    // The requests replaced the synth's edit buffer - put the editor's sound back.
    controller.sendToSynth();

    if (finishedCallback != nullptr)
        finishedCallback (std::move (result), complete);
}

} // namespace sso
