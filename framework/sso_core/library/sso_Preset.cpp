namespace sso
{

static constexpr int presetFormatVersion = 1;

juce::var Preset::toVar (const DeviceDefinition* device) const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("format", "sso-preset");
    o->setProperty ("version", presetFormatVersion);
    o->setProperty ("device", deviceId);
    o->setProperty ("name", name);
    o->setProperty ("category", category);

    juce::Array<juce::var> tagList;
    for (auto& t : tags)
        tagList.add (t);

    o->setProperty ("tags", tagList);
    o->setProperty ("author", author);
    o->setProperty ("comment", comment);
    o->setProperty ("favorite", favorite);
    o->setProperty ("rating", rating);
    o->setProperty ("created", created.toISO8601 (true));
    o->setProperty ("modified", modified.toISO8601 (true));
    o->setProperty ("origin", origin);
    o->setProperty ("sortIndex", sortIndex);
    o->setProperty ("data", patch.toBase64());

    if (device != nullptr && patch.data.size() == (size_t) device->getPatchSize())
    {
        auto* params = new juce::DynamicObject();

        for (size_t i = 0; i < device->getParams().size(); ++i)
        {
            auto& spec = device->getParams()[i];
            params->setProperty (spec.id, spec.toText (device->getValue (patch, (int) i)));
        }

        o->setProperty ("params", juce::var (params));
    }

    return juce::var (o);
}

std::optional<Preset> Preset::fromVar (const juce::var& v)
{
    if (v.getProperty ("format", {}).toString() != "sso-preset")
        return std::nullopt;

    Preset p;
    p.deviceId  = v.getProperty ("device", {}).toString();
    p.name      = v.getProperty ("name", {}).toString();
    p.category  = v.getProperty ("category", {}).toString();
    p.author    = v.getProperty ("author", {}).toString();
    p.comment   = v.getProperty ("comment", {}).toString();
    p.favorite  = (bool) v.getProperty ("favorite", false);
    p.rating    = juce::jlimit (0, 5, (int) v.getProperty ("rating", 0));
    p.created   = juce::Time::fromISO8601 (v.getProperty ("created", {}).toString());
    p.modified  = juce::Time::fromISO8601 (v.getProperty ("modified", {}).toString());
    p.origin    = v.getProperty ("origin", {}).toString();
    p.sortIndex = (int) v.getProperty ("sortIndex", -1);
    p.patch     = Patch::fromBase64 (v.getProperty ("data", {}).toString());

    if (auto* tagList = v.getProperty ("tags", {}).getArray())
        for (auto& t : *tagList)
            p.tags.add (t.toString());

    if (! p.patch.isValid())
        return std::nullopt;

    return p;
}

bool Preset::saveTo (const juce::File& f, const DeviceDefinition* device) const
{
    auto text = juce::JSON::toString (toVar (device), juce::JSON::FormatOptions{}.withSpacing (juce::JSON::Spacing::multiLine));
    return f.getParentDirectory().createDirectory() && f.replaceWithText (text, false, false, "\n");
}

std::optional<Preset> Preset::load (const juce::File& f)
{
    auto parsed = juce::JSON::parse (f);

    if (auto p = fromVar (parsed))
    {
        p->file = f;
        return p;
    }

    return std::nullopt;
}

//==============================================================================
juce::StringArray Categories::all()
{
    return { "Bass", "Lead", "Poly", "Pads", "Strings", "Brass", "Keys", "Organ", "Bells", "FX", "Perc", "Other" };
}

juce::String Categories::guessFromName (const juce::String& rawName)
{
    auto name = rawName.toLowerCase();

    struct Rule { const char* category; std::initializer_list<const char*> words; };

    static const Rule rules[] =
    {
        { "Bass",    { "bass" } },
        { "Strings", { "string", "violin", "cello", "viola", "orchestra" } },
        { "Brass",   { "brass", "horn", "trumpet", "trombone", "tuba" } },
        { "Organ",   { "organ", "accordian", "accordion", "harmonium" } },
        { "Keys",    { "piano", "clav", "harpsichord", "keys", "rhodes", "wurli", "ep " } },
        { "Bells",   { "bell", "vibraphone", "chime", "celesta", "marimba", "glock", "kalimba" } },
        { "Pads",    { "pad", "voice", "choir", "vox" } },
        { "Lead",    { "lead", "flute", "oboe", "whistle", "clarinet", "sax", "solo", "guitar" } },
        { "Perc",    { "perc", "drum", "pluck", "tom", "snare", "kick" } },
        { "FX",      { "fx", "jet", "planet", "space", "noise", "wind", "sync", "sweep", "laser", "zap" } },
        { "Poly",    { "poly", "synth", "funk", "fifth", "pulser", "wah" } },
    };

    for (auto& rule : rules)
        for (auto* word : rule.words)
            if (name.contains (word))
                return rule.category;

    return "Other";
}

} // namespace sso
