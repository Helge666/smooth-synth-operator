namespace sso
{

int DeviceDefinition::indexOf (const juce::String& paramId) const
{
    for (size_t i = 0; i < params.size(); ++i)
        if (params[i].id == paramId)
            return (int) i;

    return -1;
}

juce::StringArray DeviceDefinition::getSections() const
{
    juce::StringArray sections;

    for (auto& p : params)
        sections.addIfNotAlreadyThere (p.section);

    return sections;
}

Patch DeviceDefinition::createInitPatch() const
{
    Patch p;
    p.data.assign ((size_t) getPatchSize(), 0);

    for (size_t i = 0; i < params.size(); ++i)
        setValue (p, (int) i, params[i].defaultValue);

    setPatchName (p, "Init");
    return p;
}

void DeviceDefinition::setHelp (std::initializer_list<std::pair<const char*, const char*>> texts)
{
    for (auto& [id, text] : texts)
    {
        auto index = indexOf (id);
        jassert (index >= 0);   // unknown parameter id

        if (index >= 0)
            params[(size_t) index].help = text;
    }
}

Patch DeviceDefinition::createPatchWith (std::initializer_list<std::pair<const char*, int>> values) const
{
    auto p = createInitPatch();

    for (auto& [id, value] : values)
    {
        auto index = indexOf (id);
        jassert (index >= 0);   // unknown parameter id

        if (index >= 0)
            setValue (p, index, value);
    }

    return p;
}

static int fieldMask (const ParamSpec& s) noexcept { return ((1 << s.bits) - 1) << s.shift; }

int DeviceDefinition::readField (const ParamSpec& s, const std::vector<uint8_t>& data)
{
    if (! juce::isPositiveAndBelow (s.offset, (int) data.size()))
        return s.defaultValue;

    auto stored = (data[(size_t) s.offset] & fieldMask (s)) >> s.shift;

    if (s.isWide() && juce::isPositiveAndBelow (s.loOffset, (int) data.size()))
        stored = (stored << 7) | (data[(size_t) s.loOffset] & 0x7f);

    return s.fromStored (stored);
}

void DeviceDefinition::writeField (const ParamSpec& s, std::vector<uint8_t>& data, int value)
{
    if (! juce::isPositiveAndBelow (s.offset, (int) data.size()))
        return;

    auto stored = s.toStored (value);

    if (s.isWide() && juce::isPositiveAndBelow (s.loOffset, (int) data.size()))
    {
        data[(size_t) s.loOffset] = (uint8_t) (stored & 0x7f);
        stored >>= 7;
    }

    auto& byte = data[(size_t) s.offset];
    auto mask = fieldMask (s);
    byte = (uint8_t) ((byte & ~mask) | ((stored << s.shift) & mask));
}

int DeviceDefinition::getValue (const Patch& patch, int paramIndex) const
{
    return readField (params[(size_t) paramIndex], patch.data);
}

void DeviceDefinition::setValue (Patch& patch, int paramIndex, int value) const
{
    writeField (params[(size_t) paramIndex], patch.data, value);
}

int DeviceDefinition::getGlobalValue (const std::vector<uint8_t>& globalDump, int globalIndex) const
{
    return readField (globalParams[(size_t) globalIndex], globalDump);
}

int DeviceDefinition::indexOfGlobal (const juce::String& id) const
{
    for (size_t i = 0; i < globalParams.size(); ++i)
        if (globalParams[i].id == id)
            return (int) i;

    return -1;
}

DeviceDefinition::SlotLayout DeviceDefinition::getSlotLayout() const
{
    return { getNumSlots() / 16, 1, 16, 4 };
}

juce::String DeviceDefinition::getBankName (int bank) const
{
    auto layout = getSlotLayout();
    return getSlotName (bank * layout.groupsPerBank * layout.slotsPerGroup).upToFirstOccurrenceOf ("-", false, false);
}

juce::String DeviceDefinition::getGroupName (int group) const
{
    return juce::String (group + 1);
}

int DeviceDefinition::buildFieldByte (int offset, const std::function<int (int)>& valueOf) const
{
    return buildFieldByte (params, offset, valueOf);
}

int DeviceDefinition::buildFieldByte (const std::vector<ParamSpec>& specs, int offset, const std::function<int (int)>& valueOf)
{
    int byte = 0;

    for (size_t i = 0; i < specs.size(); ++i)
    {
        auto& s = specs[i];

        if (s.offset == offset)
            byte |= (s.toStored (valueOf ((int) i)) << s.shift) & fieldMask (s);
    }

    return byte & 0x7f;
}

} // namespace sso
