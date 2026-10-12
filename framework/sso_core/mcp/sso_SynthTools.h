#pragma once

namespace sso
{

/** The tools an AI gets for one programmer (see McpServer): read the synth's description and
    current sound, set parameters, play notes, use the library and the generator.

    Everything goes through the same paths as the user's own actions: parameters through the
    plugin parameters (GUI, host and synth follow), sounds into the synth's edit buffer only.
    Nothing is ever written to the synth's memory or its global settings, and the library is
    the only place files are written.
*/
class SynthTools
{
public:
    struct Context
    {
        juce::String appName;           // "SSO Hawk-800"
        juce::String appVersion;        // "1.0.0"
        SynthController& controller;
        PresetLibrary& library;
        PatchGenerator* generator = nullptr;
    };

    explicit SynthTools (Context);
    ~SynthTools();

    /** Registers all tools with the server. */
    void addTo (McpServer&);

    /** For the server's instructions. */
    juce::String instructions() const;

    // The tool functions (message thread) - public for tests
    McpServer::ToolResult synthInfo (const juce::var&);
    McpServer::ToolResult getSound (const juce::var&);
    McpServer::ToolResult setParameters (const juce::var&);
    McpServer::ToolResult setSoundName (const juce::var&);
    McpServer::ToolResult initSound (const juce::var&);
    McpServer::ToolResult playNotes (const juce::var&);
    McpServer::ToolResult stopSound (const juce::var&);
    McpServer::ToolResult listPresets (const juce::var&);
    McpServer::ToolResult loadPreset (const juce::var&);
    McpServer::ToolResult savePreset (const juce::var&);
    McpServer::ToolResult generateSound (const juce::var&);
    McpServer::ToolResult varySound (const juce::var&);

    /** "C4" (= 60, scientific pitch: A4 = 69 = 440 Hz), "F#3", "Bb2" or a number; -1 if invalid. */
    static int parseNote (const juce::var&);

    /** A parameter value from a number, a choice name or true/false; nullopt if it fits nowhere. */
    static std::optional<int> parseValue (const ParamSpec&, const juce::var&);

    /** "0..99", "Saw/Pulse/Square", "off/on", "0..126, 63 = centre". */
    static juce::String rangeText (const ParamSpec&);

    static constexpr double maxPhraseMs = 30000.0;
    static constexpr int maxNotes = 128;
    static constexpr const char* aiBank = "AI Sounds";

private:
    std::optional<McpServer::ToolResult> refuseWhileBusy() const;
    juce::String valueText (int paramIndex, int value) const;
    juce::String soundSummary() const;
    const Preset* findPreset (const juce::String& idOrName, juce::String& problem) const;
    juce::String presetId (const Preset&) const;
    juce::StringArray keepGroups (const juce::var&, juce::String& problem) const;
    void applyGenerated (const Patch&, const juce::String& category);

    Context context;
    const DeviceDefinition& device;
    TestNotePlayer player;
    juce::Random random;
};

} // namespace sso
