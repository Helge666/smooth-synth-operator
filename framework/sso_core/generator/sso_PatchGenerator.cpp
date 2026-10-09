namespace sso
{

int PatchGenerator::Rule::draw (juce::Random& random) const
{
    if (type == Type::range)
        return low + (high > low ? random.nextInt (high - low + 1) : 0);

    float total = 0;
    for (auto& w : weighted)
        total += w.second;

    auto x = random.nextFloat() * total;

    for (auto& w : weighted)
    {
        if (x < w.second)
            return w.first;

        x -= w.second;
    }

    return weighted.empty() ? 0 : weighted.back().first;
}

PatchGenerator::PatchGenerator (const DeviceDefinition& d) : device (d) {}

void PatchGenerator::setBaseRules (RuleSet rules)
{
    baseRules = std::move (rules);
}

void PatchGenerator::addCategory (const juce::String& name, RuleSet overrides, juce::StringArray nameWords)
{
    categories.emplace_back (name, std::move (overrides), std::move (nameWords));
}

void PatchGenerator::assignGroup (const juce::String& section, const juce::String& group)
{
    groupOfSection[section] = group;
    groups.addIfNotAlreadyThere (group);
}

juce::StringArray PatchGenerator::getCategories() const
{
    juce::StringArray names;

    for (auto& c : categories)
        names.add (std::get<0> (c));

    return names;
}

juce::String PatchGenerator::getGroupOf (int paramIndex) const
{
    auto it = groupOfSection.find (device.getParams()[(size_t) paramIndex].section);
    return it != groupOfSection.end() ? it->second : juce::String();
}

Patch PatchGenerator::generate (const juce::String& category, const Patch& current, const juce::StringArray& keepGroups,
                                juce::Random& random) const
{
    const RuleSet* overrides = nullptr;

    for (auto& c : categories)
        if (std::get<0> (c) == category)
            overrides = &std::get<1> (c);

    auto patch = current.isValid() ? current : device.createInitPatch();
    auto& params = device.getParams();

    for (size_t i = 0; i < params.size(); ++i)
    {
        if (keepGroups.contains (getGroupOf ((int) i)))
            continue;

        auto& id = params[i].id;
        const Rule* rule = nullptr;

        if (overrides != nullptr)
            if (auto it = overrides->find (id); it != overrides->end())
                rule = &it->second;

        if (rule == nullptr)
            if (auto it = baseRules.find (id); it != baseRules.end())
                rule = &it->second;

        auto value = rule != nullptr ? rule->draw (random) : params[i].defaultValue;
        device.setValue (patch, (int) i, juce::jlimit (params[i].minValue, params[i].maxValue, value));
    }

    if (sanitise != nullptr)
        sanitise (patch, random);

    device.setPatchName (patch, makeName (category, random));
    return patch;
}

Patch PatchGenerator::vary (const Patch& current, float amount, const juce::StringArray& keepGroups, juce::Random& random) const
{
    auto patch = current.isValid() ? current : device.createInitPatch();
    auto& params = device.getParams();
    amount = juce::jlimit (0.0f, 1.0f, amount);

    for (size_t i = 0; i < params.size(); ++i)
    {
        auto& spec = params[i];

        if (keepGroups.contains (getGroupOf ((int) i)))
            continue;

        auto value = device.getValue (patch, (int) i);

        if (spec.isDiscrete())
        {
            // Only waveforms change, and rarely - other switches change the character too much.
            if (spec.id.contains ("wave") && random.nextFloat() < amount * 0.25f)
                value = spec.minValue + random.nextInt (spec.numValues());
        }
        else
        {
            // Values at the minimum are usually "off" (no mod, no delay): leave them off.
            if (value == spec.minValue && spec.kind != ParamSpec::Kind::bipolar)
                continue;

            auto spread = amount * (float) (spec.maxValue - spec.minValue) * 0.35f;
            value += juce::roundToInt ((random.nextFloat() * 2.0f - 1.0f) * spread);
        }

        device.setValue (patch, (int) i, juce::jlimit (spec.minValue, spec.maxValue, value));
    }

    if (sanitise != nullptr)
        sanitise (patch, random);

    return patch;
}

juce::String PatchGenerator::makeName (const juce::String& category, juce::Random& random) const
{
    static const juce::StringArray adjectives { "Warm", "Dusty", "Velvet", "Glass", "Rubber", "Bright", "Dark", "Silk",
                                                "Analog", "Hollow", "Golden", "Neon", "Soft", "Wild", "Cold", "Fat",
                                                "Smooth", "Vintage", "Deep", "Shiny", "Lazy", "Punchy", "Misty", "Grainy" };
    juce::StringArray nouns;

    for (auto& c : categories)
        if (std::get<0> (c) == category)
            nouns = std::get<2> (c);

    if (nouns.isEmpty())
        nouns.add (category);

    auto name = adjectives[random.nextInt (adjectives.size())] + " " + nouns[random.nextInt (nouns.size())];
    return name.substring (0, device.getMaxNameLength());
}

} // namespace sso
