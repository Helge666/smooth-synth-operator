namespace sso
{

static constexpr double takeOverTimeoutMs = 3000.0;

OwnershipRegistry::OwnershipRegistry()
{
    startTimer (200);
}

OwnershipRegistry::~OwnershipRegistry()
{
    stopTimer();

    for (auto& [key, slot] : slots)
        if (slot.lock != nullptr)
            release (key, slot);
}

juce::String OwnershipRegistry::makeKey (const juce::String& deviceId, const juce::String& portId, int channel)
{
    if (portId.isEmpty())
        return {};

    // Port identifiers contain characters that are not allowed in lock/file names
    return deviceId + "-" + juce::String::toHexString ((juce::int64) portId.hashCode64()) + "-ch" + juce::String (channel);
}

juce::File OwnershipRegistry::fileFor (const juce::String& key, const char* suffix) const
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory)
               .getChildFile ("SSO-ownership").getChildFile (key + suffix);
}

juce::String OwnershipRegistry::keyOf (const Member& m) const
{
    for (auto& [member, key] : members)
        if (member == &m)
            return key;

    return {};
}

bool OwnershipRegistry::tryAcquire (const juce::String& key, Slot& slot)
{
    if (slot.lock != nullptr)
        return true;

    auto lock = std::make_unique<juce::InterProcessLock> ("SSO-" + key);

    if (! lock->enter (0))
        return false;

    slot.lock = std::move (lock);
    return true;
}

void OwnershipRegistry::release (const juce::String& key, Slot& slot)
{
    if (slot.lock == nullptr)
        return;

    fileFor (key, ".owner").deleteFile();
    slot.lock->exit();
    slot.lock.reset();
}

void OwnershipRegistry::writeOwnerFile (const juce::String& key, const Slot& slot) const
{
    if (slot.owner == nullptr)
        return;

    auto f = fileFor (key, ".owner");
    f.getParentDirectory().createDirectory();
    f.replaceWithText (slot.owner->getInstanceDescription());
}

void OwnershipRegistry::notifyAll (const juce::String& key)
{
    // copy: callbacks may change membership
    auto copy = members;

    for (auto& [member, memberKey] : copy)
        if (memberKey == key)
            member->ownershipChanged (slots[key].owner == member, false);
}

void OwnershipRegistry::add (Member& m)
{
    auto key = m.getOwnershipKey();
    members.emplace_back (&m, key);

    if (key.isEmpty())
    {
        m.ownershipChanged (true, false);   // no synth = nothing to share
        return;
    }

    auto& slot = slots[key];

    // First come, first served: become owner if nobody in this or another process is
    if (slot.owner == nullptr && tryAcquire (key, slot))
    {
        slot.owner = &m;
        writeOwnerFile (key, slot);
    }

    m.ownershipChanged (slot.owner == &m, false);
}

void OwnershipRegistry::remove (Member& m)
{
    auto key = keyOf (m);
    members.erase (std::remove_if (members.begin(), members.end(), [&m] (auto& p) { return p.first == &m; }), members.end());

    if (key.isEmpty())
        return;

    auto& slot = slots[key];

    if (slot.requester == &m)
        slot.requester = nullptr;

    if (slot.owner == &m)
    {
        // The synth becomes free. Other instances stay inactive until the user takes over -
        // switching sounds on the synth without being asked would be surprising.
        slot.owner = nullptr;
        release (key, slot);
        notifyAll (key);
    }
}

void OwnershipRegistry::keyChanged (Member& m)
{
    remove (m);
    add (m);
}

void OwnershipRegistry::descriptionChanged (Member& m)
{
    auto key = keyOf (m);

    if (key.isNotEmpty() && slots[key].owner == &m)
        writeOwnerFile (key, slots[key]);

    notifyAll (key);
}

bool OwnershipRegistry::isOwner (const Member& m) const
{
    auto key = keyOf (m);

    if (key.isEmpty())
        return true;

    auto it = slots.find (key);
    return it != slots.end() && it->second.owner == &m;
}

bool OwnershipRegistry::isTakingOver (const Member& m) const
{
    auto it = slots.find (keyOf (m));
    return it != slots.end() && it->second.requester == &m;
}

juce::String OwnershipRegistry::getOwnerDescription (const Member& m) const
{
    auto key = keyOf (m);
    auto it = slots.find (key);

    if (key.isEmpty() || it == slots.end())
        return {};

    if (it->second.owner != nullptr)
        return it->second.owner == &m ? juce::String() : it->second.owner->getInstanceDescription();

    // Owned by another process?
    auto f = fileFor (key, ".owner");
    return f.existsAsFile() ? f.loadFileAsString().trim() : juce::String();
}

void OwnershipRegistry::takeOver (Member& m)
{
    auto key = keyOf (m);

    if (key.isEmpty())
        return;

    auto& slot = slots[key];

    if (slot.owner == &m)
        return;

    if (slot.owner != nullptr || tryAcquire (key, slot))
    {
        // In this process (or the synth was free): switch directly
        auto* previous = slot.owner;
        slot.owner = &m;
        writeOwnerFile (key, slot);

        if (previous != nullptr)
            previous->ownershipChanged (false, false);

        m.ownershipChanged (true, true);
        notifyAll (key);
        return;
    }

    // Another process owns it: ask it to let go
    auto request = fileFor (key, ".takeover");
    request.getParentDirectory().createDirectory();
    request.replaceWithText (processToken);
    slot.requester = &m;
    slot.requestStarted = juce::Time::getMillisecondCounterHiRes();
    notifyAll (key);
}

void OwnershipRegistry::timerCallback()
{
    for (auto& [key, slot] : slots)
    {
        auto request = fileFor (key, ".takeover");

        // We own it and another process asks for it (recently - stale requests of crashed
        // processes are ignored): release
        auto requestIsFresh = request.existsAsFile()
                               && juce::Time::getCurrentTime() - request.getLastModificationTime()
                                      < juce::RelativeTime::milliseconds ((juce::int64) takeOverTimeoutMs + 1000);

        if (slot.lock != nullptr && requestIsFresh && request.loadFileAsString() != processToken)
        {
            auto* previous = slot.owner;
            slot.owner = nullptr;
            release (key, slot);

            if (previous != nullptr)
                previous->ownershipChanged (false, false);

            notifyAll (key);
            continue;
        }

        // We asked for it: try to get it
        if (slot.requester != nullptr)
        {
            if (tryAcquire (key, slot))
            {
                request.deleteFile();
                auto* m = slot.requester;
                slot.requester = nullptr;
                slot.owner = m;
                writeOwnerFile (key, slot);
                m->ownershipChanged (true, true);
                notifyAll (key);
            }
            else if (juce::Time::getMillisecondCounterHiRes() - slot.requestStarted > takeOverTimeoutMs)
            {
                request.deleteFile();
                slot.requester = nullptr;
                notifyAll (key);
            }
        }
    }
}

} // namespace sso
