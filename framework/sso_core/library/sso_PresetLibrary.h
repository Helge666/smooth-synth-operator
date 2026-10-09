#pragma once

namespace sso
{

/** All presets of one device, stored as files below a root folder.
    Each sub folder is a bank (e.g. "Kiwi Factory", "My Sounds").

    Default root: Documents/Smooth Synth Operator/<device folder>/Presets
*/
class PresetLibrary : public juce::ChangeBroadcaster
{
public:
    explicit PresetLibrary (const DeviceDefinition&, juce::File root = {});

    static juce::File getDefaultRoot (const DeviceDefinition&);
    const juce::File& getRoot() const noexcept { return root; }

    void rescan();
    const std::vector<Preset>& getPresets() const noexcept { return presets; }
    juce::StringArray getBanks() const;

    struct Filter
    {
        juce::String text;          // matches name, tags, category, author, comment
        juce::String bank;          // empty = all
        juce::String category;      // empty = all
        bool favoritesOnly = false;
    };

    /** Indices into getPresets(), sorted by bank, original order, then name. */
    std::vector<int> search (const Filter&) const;

    /** Saves a new preset into `bank`; returns the file or an empty File on failure. */
    juce::File add (Preset, const juce::String& bank);

    /** Rewrites an existing preset's file (metadata and/or sound). */
    bool update (const Preset&);

    /** Moves the preset's file to the OS trash. */
    bool remove (const Preset&);

    struct ImportResult { int imported = 0, duplicates = 0, rejected = 0; };
    using ImportFilter = std::function<bool (const DeviceDefinition::IncomingPatch&)>;

    /** Adds received/parsed patches to `bank`. Sounds identical to one already in that
        bank are skipped. `source` is recorded as the presets' origin. */
    ImportResult importPatches (const std::vector<DeviceDefinition::IncomingPatch>&, const juce::String& bank,
                                const juce::String& source, const ImportFilter& accept = nullptr,
                                const juce::String& author = {});

    /** Imports all patch dumps of a .syx file. */
    ImportResult importSyx (const juce::File& syx, const juce::String& bank,
                            const ImportFilter& accept = nullptr, const juce::String& author = {});

    /** All patch dumps contained in a .syx file, regardless of their device id. */
    std::vector<DeviceDefinition::IncomingPatch> parseSyx (const juce::File&) const;

    /** Splits a .syx file into its SysEx messages. */
    static std::vector<juce::MidiMessage> readSyxFile (const juce::File&);

private:
    bool containsPatch (const juce::String& bank, const Patch&) const;

    const DeviceDefinition& device;
    juce::File root;
    std::vector<Preset> presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetLibrary)
};

} // namespace sso
