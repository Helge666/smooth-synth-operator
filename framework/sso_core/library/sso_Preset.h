#pragma once

namespace sso
{

/** A sound in the library: the raw patch plus everything the hardware can't store.

    Stored as one JSON file (*.ssop). `data` (the raw patch, base64) is authoritative;
    the readable `params` block is written for humans and diff tools and ignored on load.
*/
struct Preset
{
    juce::String name;
    juce::String category;
    juce::StringArray tags;
    juce::String author;
    juce::String comment;
    bool favorite = false;
    int rating = 0;                 // 0-5
    juce::Time created, modified;
    juce::String deviceId;
    juce::String origin;            // where it came from, e.g. "Synth A-1" or "Generator: Bass"
    int sortIndex = -1;             // keeps original bank order (e.g. synth slot), -1 = by name
    Patch patch;

    // Set by the library, not stored in the file
    juce::File file;
    juce::String bank;              // folder relative to the library root

    juce::var toVar (const DeviceDefinition* deviceForReadableParams = nullptr) const;
    static std::optional<Preset> fromVar (const juce::var&);

    bool saveTo (const juce::File&, const DeviceDefinition* deviceForReadableParams = nullptr) const;
    static std::optional<Preset> load (const juce::File&);

    static constexpr const char* fileExtension = ".ssop";
};

/** Common sound categories, shared by all devices. */
struct Categories
{
    static juce::StringArray all();

    /** Best guess from a sound's name, e.g. "Electric Piano 2" -> "Keys". */
    static juce::String guessFromName (const juce::String& name);
};

} // namespace sso
