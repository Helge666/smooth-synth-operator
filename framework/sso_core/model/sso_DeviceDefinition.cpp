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

static int fieldMask (const ParamSpec& s) noexcept { return ((1 << s.bits) - 1) << s.shift; }

int DeviceDefinition::getValue (const Patch& patch, int paramIndex) const
{
    auto& s = params[(size_t) paramIndex];

    if (! juce::isPositiveAndBelow (s.offset, (int) patch.data.size()))
        return s.defaultValue;

    auto stored = (patch.data[(size_t) s.offset] & fieldMask (s)) >> s.shift;
    return s.fromStored (stored);
}

void DeviceDefinition::setValue (Patch& patch, int paramIndex, int value) const
{
    auto& s = params[(size_t) paramIndex];

    if (! juce::isPositiveAndBelow (s.offset, (int) patch.data.size()))
        return;

    auto& byte = patch.data[(size_t) s.offset];
    auto mask = fieldMask (s);
    byte = (uint8_t) ((byte & ~mask) | ((s.toStored (value) << s.shift) & mask));
}

int DeviceDefinition::buildFieldByte (int offset, const std::function<int (int)>& valueOf) const
{
    int byte = 0;

    for (size_t i = 0; i < params.size(); ++i)
    {
        auto& s = params[i];

        if (s.offset == offset)
            byte |= (s.toStored (valueOf ((int) i)) << s.shift) & fieldMask (s);
    }

    return byte & 0x7f;
}

} // namespace sso
