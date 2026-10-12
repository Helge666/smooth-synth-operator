namespace sso::hawk800
{

namespace
{
    // The original Korg Poly-800 sounds, programs 11-88 (Korg owner's manual). The HAWK
    // factory bank 2 holds them in this order (verified by ear on 2026-10-12).
    const char* const korgNames[64] = {
        "Strings", "Acoustic Piano", "Synthe Brass", "Flute with Chiff", "Resonant Sweep", "Bowed Cellos", "Full Organ", "Fat Synthe",
        "Percussive Synthe", "Muted Brass", "Square Wave Lead", "1999", "Low Strings", "Percussive Synthe 2", "Bright Perc. Organ", "Science Bass",
        "\"Lambda\"", "OB Cross", "High Strings", "Double Crescendo Bass", "Synthe Bass I", "Reverse Bowed Synthe", "Jazz Guitar", "Helicopter",
        "Double Tongued Bass", "Synful Eyes", "\"Wet Ones\"", "Science Strings", "Synthe Bass II", "Crescendo Brass", "Chorus Piano", "Jet Plane",
        "Synthenelli", "Synthe Flute/Brass", "Synthicord I", "Synthicord II", "Saxo-Brass", "Square Wave Ensemble", "Chime Piano", "Pipe Organ",
        "Bowed Violas", "Digi-Brass", "Organ Upper Manual", "Obi Brass", "Soft Piano", "Reverse Sweep", "Accordion", "Steel Drums",
        "8-Note Brass", "Honky Keys", "Organ Lower Manual", "8-Note Strings", "Reso Synthe", "Echo Synthe", "Black Hole", "Trilling Pipes",
        "Brass in 5ths", "Percussive Pipe Organ", "Lunar Flutes", "Synthe Bass III", "Vibes", "Low Flute", "\"Who Me?\"", "UFO Swoop" };

    // Library categories of the Korg sounds, in the same order
    const char* const korgCategories[64] = {
        "Strings", "Keys", "Brass", "Lead", "FX", "Strings", "Organ", "Poly",
        "Keys", "Brass", "Lead", "Poly", "Strings", "Keys", "Organ", "Bass",
        "Pads", "Brass", "Strings", "Bass", "Bass", "FX", "Keys", "FX",
        "Bass", "Pads", "FX", "Strings", "Bass", "Brass", "Keys", "FX",
        "Poly", "Lead", "Poly", "Poly", "Brass", "Pads", "Keys", "Organ",
        "Strings", "Brass", "Organ", "Brass", "Keys", "FX", "Organ", "Perc",
        "Brass", "Keys", "Organ", "Strings", "Poly", "Poly", "FX", "Lead",
        "Brass", "Organ", "Pads", "Bass", "Bells", "Lead", "FX", "FX" };

    constexpr int originalParams = 64;     // P1 11-88: the parameters the original Poly-800 has
}

juce::String getKorgName (int slotInBank)
{
    return juce::isPositiveAndBelow (slotInBank, 64) ? juce::String (korgNames[slotInBank]) : juce::String();
}

PresetLibrary::ImportResult importFactoryBanks (PresetLibrary& library, const Hawk800Device& device,
                                                const juce::File& folder, const juce::String& hawkBank,
                                                const juce::String& korgBank)
{
    std::array<std::vector<DeviceDefinition::IncomingPatch>, Hawk800Device::numBanks> banks;

    for (int b = 0; b < Hawk800Device::numBanks; ++b)
        for (auto& m : PresetLibrary::readSyxFile (folder.getChildFile ("Bank" + juce::String (b + 1) + ".syx")))
            if (auto bulk = device.parseBulkDump ({}, m); bulk.size() == (size_t) Hawk800Device::slotsPerBank)
                banks[(size_t) b] = std::move (bulk);

    for (auto& bank : banks)
        if (bank.empty())
            return {};

    auto originalPart = [] (const Patch& p) { return std::vector<uint8_t> (p.data.begin(), p.data.begin() + originalParams); };

    // The Korg sounds by their original parameters
    std::map<std::vector<uint8_t>, int> korgSlotOf;

    for (int i = 0; i < Hawk800Device::slotsPerBank; ++i)
        korgSlotOf.emplace (originalPart (banks[1][(size_t) i].patch), i);

    // Bank 2 first, so that the Korg sounds keep their plain names. A sound whose original
    // parameters equal a Korg sound is a HAWK variation of it: "<name> A", "B", ... Other
    // sounds are named after their place, "Sound <bank>-<number>". Exact copies are left out.
    std::vector<DeviceDefinition::IncomingPatch> korgSounds, hawkSounds;
    std::vector<Patch> seen;
    std::map<int, int> variations;
    std::map<juce::String, juce::String> categoryOf;     // name -> category, for the Korg sounds and their variations

    for (auto b : { 1, 0, 2, 3 })
    {
        for (int i = 0; i < Hawk800Device::slotsPerBank; ++i)
        {
            auto incoming = banks[(size_t) b][(size_t) i];
            auto sound = device.asStored (incoming.patch);

            if (std::find (seen.begin(), seen.end(), sound) != seen.end())
                continue;

            seen.push_back (sound);
            incoming.slot = b * Hawk800Device::slotsPerBank + i;
            juce::String name;

            if (b == 1)
            {
                name = korgNames[i];
                categoryOf[name] = korgCategories[i];
            }
            else if (auto it = korgSlotOf.find (originalPart (incoming.patch)); it != korgSlotOf.end())
            {
                name = juce::String (korgNames[it->second]) + " " + juce::String::charToString ((juce::juce_wchar) ('A' + variations[it->second]++));
                categoryOf[name] = korgCategories[it->second];
            }
            else
                name = "Sound " + device.getSlotName (incoming.slot);

            device.setPatchName (incoming.patch, name);
            (b == 1 ? korgSounds : hawkSounds).push_back (std::move (incoming));
        }
    }

    auto result = library.importPatches (korgSounds, korgBank, "HAWK-800 factory bank 2", nullptr, "Korg");
    result.imported += library.importPatches (hawkSounds, hawkBank, "HAWK-800 factory banks", nullptr, "HAWK-800").imported;

    auto presets = library.getPresets();     // a copy: update() rescans the library

    for (auto preset : presets)
    {
        if (auto it = categoryOf.find (preset.name); it != categoryOf.end() && (preset.bank == hawkBank || preset.bank == korgBank)
             && preset.category != it->second)
        {
            preset.category = it->second;
            library.update (preset);
        }
    }

    // Our starting point for programming: the generator's INIT
    if (! library.getPresets().empty())
    {
        auto init = device.createInitPatch();
        bool exists = false;

        for (auto& p : library.getPresets())
            exists = exists || (p.bank == hawkBank && p.patch == init);

        if (! exists)
        {
            Preset p;
            p.name = "Init";
            p.deviceId = device.getDeviceId();
            p.origin = "SSO Hawk-800";
            p.created = p.modified = juce::Time::getCurrentTime();
            p.sortIndex = 0;        // first, before "Sound 1-11"
            p.patch = init;

            if (library.add (p, hawkBank).existsAsFile())
                ++result.imported;
        }
    }

    return result;
}

} // namespace sso::hawk800
