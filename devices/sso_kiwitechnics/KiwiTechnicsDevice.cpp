namespace sso::kiwitechnics
{

namespace
{
    // F0 [00 21 16] KiwiTechnics, [60] Kiwi, [model] [dev] [cmd] ... F7
    constexpr uint8_t manufacturer[] = { 0x00, 0x21, 0x16, 0x60 };
    constexpr int headerSize = (int) sizeof (manufacturer) + 1;
}

KiwiTechnicsDevice::KiwiTechnicsDevice (uint8_t model) : modelId (model) {}

juce::String KiwiTechnicsDevice::getPatchName (const Patch& p) const
{
    juce::String name;

    for (int i = 0; i < nameLength && i < (int) p.data.size(); ++i)
    {
        auto c = p.data[(size_t) i];
        name << juce::String::charToString ((juce::juce_wchar) (c >= 32 && c < 127 ? c : ' '));
    }

    return name.trimEnd();
}

void KiwiTechnicsDevice::setPatchName (Patch& p, const juce::String& name) const
{
    if (p.data.size() < (size_t) nameLength)
        return;

    for (int i = 0; i < nameLength; ++i)
    {
        auto c = i < name.length() ? (int) name[i] : ' ';
        p.data[(size_t) i] = (uint8_t) (c >= 32 && c < 127 ? c : '?');
    }
}

juce::MidiMessage KiwiTechnicsDevice::createSysEx (const MidiSettings& settings, uint8_t command,
                                                   std::initializer_list<uint8_t> payload, const Patch* patch) const
{
    std::vector<uint8_t> data (std::begin (manufacturer), std::end (manufacturer));
    data.push_back (modelId);
    data.push_back ((uint8_t) (settings.deviceId & 0x0f));
    data.push_back (command);
    data.insert (data.end(), payload.begin(), payload.end());

    if (patch != nullptr)
        for (auto b : patch->data)
            data.push_back ((uint8_t) (b & 0x7f));

    return juce::MidiMessage::createSysExMessage (data.data(), (int) data.size());
}

juce::MidiMessage KiwiTechnicsDevice::createEditBufferRequest (const MidiSettings& s) const
{
    return createSysEx (s, requestEditBuffer, {});
}

juce::MidiMessage KiwiTechnicsDevice::createEditBufferDump (const MidiSettings& s, const Patch& patch) const
{
    jassert (patch.data.size() == (size_t) patchSize);
    return createSysEx (s, editBufferDump, { 0, 0 }, &patch);
}

juce::MidiMessage KiwiTechnicsDevice::createSlotRequest (const MidiSettings& s, int slot) const
{
    return createSysEx (s, requestTone, { (uint8_t) (slot >> 7), (uint8_t) (slot & 0x7f) });
}

juce::MidiMessage KiwiTechnicsDevice::createSlotDump (const MidiSettings& s, const Patch& patch, int slot) const
{
    jassert (patch.data.size() == (size_t) patchSize && juce::isPositiveAndBelow (slot, getNumSlots()));
    return createSysEx (s, toneDump, { (uint8_t) (slot >> 7), (uint8_t) (slot & 0x7f) }, &patch);
}

std::optional<DeviceDefinition::IncomingPatch> KiwiTechnicsDevice::parsePatchDump (const MidiSettings& s,
                                                                                   const juce::MidiMessage& m) const
{
    if (! m.isSysEx())
        return std::nullopt;

    auto* d = m.getSysExData();
    auto size = m.getSysExDataSize();

    if (size != headerSize + 2 + 2 + patchSize
        || std::memcmp (d, manufacturer, sizeof (manufacturer)) != 0
        || d[sizeof (manufacturer)] != modelId)
        return std::nullopt;

    if ((d[headerSize] & 0x0f) != (s.deviceId & 0x0f))
        return std::nullopt;

    auto command = d[headerSize + 1];
    IncomingPatch result;

    // The two bytes before an edit-buffer dump are not always zero (KiwiSix) - ignored.
    if (command == toneDump)
        result.slot = (d[headerSize + 2] << 7) | d[headerSize + 3];
    else if (command != editBufferDump)
        return std::nullopt;

    auto* data = d + headerSize + 4;
    result.patch.data.assign (data, data + patchSize);
    return result;
}

//==============================================================================
juce::MidiMessage KiwiTechnicsDevice::createGlobalsRequest (const MidiSettings& s) const
{
    return createSysEx (s, requestGlobals, {});
}

std::optional<std::vector<uint8_t>> KiwiTechnicsDevice::parseGlobalsDump (const MidiSettings& s, const juce::MidiMessage& m) const
{
    if (! m.isSysEx())
        return std::nullopt;

    auto* d = m.getSysExData();
    auto size = m.getSysExDataSize();

    if (size <= headerSize + 2
        || std::memcmp (d, manufacturer, sizeof (manufacturer)) != 0
        || d[sizeof (manufacturer)] != modelId
        || (d[headerSize] & 0x0f) != (s.deviceId & 0x0f)
        || d[headerSize + 1] != globalsDump)
        return std::nullopt;

    return std::vector<uint8_t> (d + headerSize + 2, d + size);
}

std::vector<juce::MidiMessage> KiwiTechnicsDevice::createGlobalMessages (const MidiSettings& s, const std::function<int (int)>& valueOf) const
{
    // One "global parameter" message per byte, built from all fields sharing it
    std::vector<juce::MidiMessage> result;
    std::set<int> offsets;

    for (auto& g : globalParams)
        offsets.insert (g.offset);

    for (auto offset : offsets)
        result.push_back (createSysEx (s, globalParameter,
                                       { (uint8_t) offset, (uint8_t) buildFieldByte (globalParams, offset, valueOf) }));

    return result;
}

} // namespace sso::kiwitechnics
