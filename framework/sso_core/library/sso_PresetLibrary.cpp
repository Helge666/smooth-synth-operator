namespace sso
{

PresetLibrary::PresetLibrary (const DeviceDefinition& d, juce::File r)
    : device (d), root (r == juce::File() ? getDefaultRoot (d) : r)
{
    rescan();
}

juce::File PresetLibrary::getDefaultRoot (const DeviceDefinition& d)
{
    return AppSettings::getDataDirectory().getChildFile (d.getLibraryFolderName()).getChildFile ("Presets");
}

void PresetLibrary::rescan()
{
    presets.clear();

    for (auto& entry : juce::RangedDirectoryIterator (root, true, juce::String ("*") + Preset::fileExtension))
    {
        auto preset = Preset::load (entry.getFile());

        if (! preset.has_value() || preset->deviceId != device.getDeviceId())
            continue;

        preset->bank = entry.getFile().getParentDirectory().getRelativePathFrom (root).replaceCharacter ('\\', '/');

        if (preset->bank == ".")
            preset->bank = {};

        presets.push_back (std::move (*preset));
    }

    sendChangeMessage();
}

juce::StringArray PresetLibrary::getBanks() const
{
    juce::StringArray banks;

    for (auto& dir : juce::RangedDirectoryIterator (root, false, "*", juce::File::findDirectories))
        banks.add (dir.getFile().getFileName());

    for (auto& p : presets)
        if (p.bank.isNotEmpty())
            banks.addIfNotAlreadyThere (p.bank);

    banks.sortNatural();
    return banks;
}

std::vector<int> PresetLibrary::search (const Filter& filter) const
{
    std::vector<int> result;
    auto words = juce::StringArray::fromTokens (filter.text.toLowerCase(), true);

    for (int i = 0; i < (int) presets.size(); ++i)
    {
        auto& p = presets[(size_t) i];

        if (filter.bank.isNotEmpty() && p.bank != filter.bank)                 continue;
        if (filter.category.isNotEmpty() && p.category != filter.category)     continue;
        if (filter.favoritesOnly && ! p.favorite)                              continue;

        auto haystack = (p.name + " " + p.category + " " + p.tags.joinIntoString (" ") + " "
                         + p.author + " " + p.comment + " " + p.bank).toLowerCase();
        bool matches = true;

        for (auto& w : words)
            matches = matches && haystack.contains (w);

        if (matches)
            result.push_back (i);
    }

    std::sort (result.begin(), result.end(), [this] (int a, int b)
    {
        auto& pa = presets[(size_t) a];
        auto& pb = presets[(size_t) b];

        if (pa.bank != pb.bank)              return pa.bank.compareNatural (pb.bank) < 0;
        if (pa.sortIndex != pb.sortIndex)    return (unsigned) pa.sortIndex < (unsigned) pb.sortIndex;   // -1 last
        return pa.name.compareNatural (pb.name) < 0;
    });

    return result;
}

juce::File PresetLibrary::add (Preset preset, const juce::String& bank)
{
    auto now = juce::Time::getCurrentTime();
    preset.deviceId = device.getDeviceId();

    if (preset.created == juce::Time())
        preset.created = now;

    preset.modified = now;

    auto folder = bank.isEmpty() ? root : root.getChildFile (juce::File::createLegalFileName (bank));
    auto baseName = juce::File::createLegalFileName (preset.name.isEmpty() ? juce::String ("Untitled") : preset.name);
    auto file = folder.getNonexistentChildFile (baseName, Preset::fileExtension, false);

    if (! preset.saveTo (file, &device))
        return {};

    rescan();
    return file;
}

bool PresetLibrary::update (const Preset& preset)
{
    if (! preset.file.existsAsFile())
        return false;

    auto copy = preset;
    copy.modified = juce::Time::getCurrentTime();

    if (! copy.saveTo (copy.file, &device))
        return false;

    rescan();
    return true;
}

bool PresetLibrary::remove (const Preset& preset)
{
    if (! preset.file.existsAsFile() || ! preset.file.moveToTrash())
        return false;

    rescan();
    return true;
}

bool PresetLibrary::containsPatch (const juce::String& bank, const Patch& patch) const
{
    for (auto& p : presets)
        if (p.bank == bank && p.patch == patch)
            return true;

    return false;
}

std::vector<juce::MidiMessage> PresetLibrary::readSyxFile (const juce::File& file)
{
    juce::MemoryBlock mb;
    std::vector<juce::MidiMessage> result;

    if (! file.loadFileAsData (mb))
        return result;

    auto* d = static_cast<const uint8_t*> (mb.getData());
    auto size = mb.getSize();

    for (size_t i = 0; i < size; ++i)
    {
        if (d[i] != 0xf0)
            continue;

        auto end = i + 1;

        while (end < size && d[end] != 0xf7)
            ++end;

        if (end < size)
            result.push_back (juce::MidiMessage::createSysExMessage (d + i + 1, (int) (end - i - 1)));

        i = end;
    }

    return result;
}

std::vector<DeviceDefinition::IncomingPatch> PresetLibrary::parseSyx (const juce::File& syx) const
{
    std::vector<DeviceDefinition::IncomingPatch> result;
    DeviceDefinition::MidiSettings anyDevice;

    for (auto& m : readSyxFile (syx))
    {
        // Dumps carry the device id they were recorded with; accept any.
        for (int id = 0; id < 16; ++id)
        {
            anyDevice.deviceId = id;

            if (auto incoming = device.parsePatchDump (anyDevice, m))
            {
                result.push_back (std::move (*incoming));
                break;
            }
        }
    }

    return result;
}

PresetLibrary::ImportResult PresetLibrary::importSyx (const juce::File& syx, const juce::String& bank,
                                                      const ImportFilter& accept, const juce::String& author)
{
    return importPatches (parseSyx (syx), bank, syx.getFileName(), accept, author);
}

PresetLibrary::ImportResult PresetLibrary::importPatches (const std::vector<DeviceDefinition::IncomingPatch>& patches,
                                                          const juce::String& bank, const juce::String& source,
                                                          const ImportFilter& accept, const juce::String& author)
{
    ImportResult result;
    auto folder = root.getChildFile (juce::File::createLegalFileName (bank));
    auto now = juce::Time::getCurrentTime();
    std::vector<Patch> addedNow;

    for (auto& incoming : patches)
    {
        if (accept != nullptr && ! accept (incoming))
        {
            ++result.rejected;
            continue;
        }

        if (containsPatch (bank, incoming.patch)
             || std::find (addedNow.begin(), addedNow.end(), incoming.patch) != addedNow.end())
        {
            ++result.duplicates;
            continue;
        }

        auto slotName = incoming.slot >= 0 ? device.getSlotName (incoming.slot) : juce::String();

        Preset p;
        p.deviceId = device.getDeviceId();
        p.name = device.getPatchName (incoming.patch);

        if (p.name.isEmpty())
            p.name = slotName.isNotEmpty() ? "Untitled " + slotName : juce::String ("Untitled");

        p.category = Categories::guessFromName (p.name);
        p.author = author;
        p.created = p.modified = now;
        p.origin = slotName.isNotEmpty() ? "Synth " + slotName + " (" + source + ")" : source;
        p.sortIndex = incoming.slot;
        p.patch = incoming.patch;

        auto file = folder.getNonexistentChildFile (juce::File::createLegalFileName (p.name), Preset::fileExtension, false);

        if (p.saveTo (file, &device))
        {
            addedNow.push_back (p.patch);
            ++result.imported;
        }
    }

    rescan();
    return result;
}

} // namespace sso
