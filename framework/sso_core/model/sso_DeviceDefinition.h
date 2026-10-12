#pragma once

namespace sso
{

/** Everything the framework needs to know about one hardware synth model.

    A device definition is stateless: it describes parameters and builds/parses MIDI
    messages. Runtime state lives in SynthController.
*/
class DeviceDefinition
{
public:
    virtual ~DeviceDefinition() = default;

    //==============================================================================
    virtual juce::String getDeviceId() const = 0;        // e.g. "kiwi3p" - stored in preset files
    virtual juce::String getDisplayName() const = 0;
    virtual juce::String getLibraryFolderName() const = 0;  // e.g. "Kiwi-3P" (Documents/Smooth Synth Operator/<this>)

    /** The synth firmware the app was built and tested with, e.g. "HAWK-800 v2.68b" - shown
        with the app's version, so users know what it was made for. */
    virtual juce::String getFirmwareInfo() const            { return {}; }

    /** The synth in a few formal lines for AI control (voices, signal flow, quirks) - see
        SynthTools' synth_info. Each line "KEY text", e.g. "FLOW DCO -> VCF -> VCA". */
    virtual juce::String getSynthSummary() const            { return {}; }

    /** Local port of the app's AI control (MCP) endpoint - one per synth, so the apps can run
        side by side. */
    virtual int getMcpPort() const                          { return 47830; }
    virtual int getPatchSize() const = 0;
    virtual int getMaxNameLength() const = 0;
    virtual int getNumSlots() const = 0;                 // stored sound memories
    virtual juce::String getSlotName (int slot) const = 0;

    /** How the stored sounds are shown: banks, optional groups within a bank (selected like
        the synth's first tone button), slots per group, tile columns. */
    struct SlotLayout
    {
        int banks = 16, groupsPerBank = 1, slotsPerGroup = 16, columns = 4;
    };

    virtual SlotLayout getSlotLayout() const;
    virtual juce::String getBankName (int bank) const;
    virtual juce::String getGroupName (int group) const;

    const std::vector<ParamSpec>& getParams() const noexcept { return params; }
    int indexOf (const juce::String& paramId) const;
    juce::StringArray getSections() const;

    //==============================================================================
    virtual Patch createInitPatch() const;
    virtual juce::String getPatchName (const Patch&) const = 0;
    virtual void setPatchName (Patch&, const juce::String&) const = 0;

    int getValue (const Patch&, int paramIndex) const;
    void setValue (Patch&, int paramIndex, int value) const;

    /** The full byte at `offset`, built from the values of all parameters sharing it.
        Used for single-parameter SysEx edits of packed bit fields. */
    int buildFieldByte (int offset, const std::function<int (int paramIndex)>& valueOf) const;

    //==============================================================================
    struct MidiSettings
    {
        int channel = 1;    // 1-16
        int deviceId = 0;   // 0-15
    };

    virtual std::vector<juce::MidiMessage> createParamMessages (const MidiSettings&, int paramIndex,
                                                                const std::function<int (int)>& valueOf) const = 0;
    virtual juce::MidiMessage createEditBufferRequest (const MidiSettings&) const = 0;
    virtual juce::MidiMessage createEditBufferDump (const MidiSettings&, const Patch&) const = 0;
    virtual juce::MidiMessage createSlotRequest (const MidiSettings&, int slot) const = 0;
    virtual juce::MidiMessage createSlotDump (const MidiSettings&, const Patch&, int slot) const = 0;

    /** The patch as the synth will hold it after storing it (some firmwares replace values
        when writing) - for verifying a write by reading it back. */
    virtual Patch asStored (const Patch& p) const   { return p; }

    /** Parameters that must be re-sent individually after an edit-buffer dump because
        the hardware does not fully apply them from the dump. */
    virtual std::vector<int> getParamsToRefreshAfterDump() const { return {}; }

    struct IncomingPatch
    {
        Patch patch;
        int slot = -1;      // -1 = edit buffer
    };

    virtual std::optional<IncomingPatch> parsePatchDump (const MidiSettings&, const juce::MidiMessage&) const = 0;

    /** All sounds of a message holding several (e.g. a whole bank in one SysEx), for importing
        files. Their `slot` counts from 0 within the message. */
    virtual std::vector<IncomingPatch> parseBulkDump (const MidiSettings&, const juce::MidiMessage&) const { return {}; }

    /** False for synths that keep no names: the name then lives only in the app's copy of
        the patch (bytes the synth never receives), and stored sounds are named by matching
        them with the library. */
    virtual bool storesPatchNames() const   { return true; }

    //==============================================================================
    /** Synths that address stored sounds only within the selected bank (HAWK-800): the bank
        of a slot, or -1 when slots are addressed directly. Slot requests and dumps then
        follow a bank select, and dumps the synth sends report the slot within the bank. */
    virtual int getSlotBank (int /*slot*/) const    { return -1; }
    virtual juce::MidiMessage createBankSelect (const MidiSettings&, int /*bank*/) const   { return {}; }

    /** The bank selected on the synth's panel, read from its global dump (-1 = unknown) -
        selected again after the app has read or written other banks. */
    virtual int getSelectedBank (const std::vector<uint8_t>& /*globalDump*/) const       { return -1; }

    //==============================================================================
    /** Tuning by ear (TUNE button): a sound with one plain oscillator and the filter fully open,
        played as `note` (A = 440 Hz when the synth is in tune), and the waveform the app plays
        as its 440 Hz reference - the same one, so the two can be compared directly. */
    struct TuneSound
    {
        enum class Wave { saw, square };

        Patch patch;
        int note = 69;
        Wave wave = Wave::saw;
    };

    virtual std::optional<TuneSound> getTuneSound() const   { return std::nullopt; }

    /** How long the synth needs after receiving a sound (or All Sound Off) before it plays notes
        reliably: notes sent earlier can get lost while its processor is busy. */
    virtual int getSettleTimeMs() const     { return 100; }

    //==============================================================================
    /** Synth-wide settings that are not part of a sound (tuning, arpeggiator clock, ...).
        Only musical ones belong here - never MIDI channels or MIDI enables, which the app
        needs to talk to the synth. Their offsets refer to the synth's global dump. */
    const std::vector<ParamSpec>& getGlobalParams() const noexcept   { return globalParams; }
    int indexOfGlobal (const juce::String& id) const;

    virtual juce::MidiMessage createGlobalsRequest (const MidiSettings&) const   { return {}; }
    virtual std::optional<std::vector<uint8_t>> parseGlobalsDump (const MidiSettings&, const juce::MidiMessage&) const
    {
        return std::nullopt;
    }

    /** Messages that set every global parameter in the synth (valueOf: global param index -> value). */
    virtual std::vector<juce::MidiMessage> createGlobalMessages (const MidiSettings&, const std::function<int (int)>&) const
    {
        return {};
    }

    int getGlobalValue (const std::vector<uint8_t>& globalDump, int globalIndex) const;

    // Field access shared by sound and global parameters
    static int readField (const ParamSpec&, const std::vector<uint8_t>& data);
    static void writeField (const ParamSpec&, std::vector<uint8_t>& data, int value);
    static int buildFieldByte (const std::vector<ParamSpec>&, int offset, const std::function<int (int)>& valueOf);

protected:
    /** Sets ParamSpec::help by parameter id (for AI control); every id must exist. */
    void setHelp (std::initializer_list<std::pair<const char*, const char*>>);

    /** The init patch with some parameters (by id) set - for sounds a device defines in code. */
    Patch createPatchWith (std::initializer_list<std::pair<const char*, int>> values) const;

    std::vector<ParamSpec> params;
    std::vector<ParamSpec> globalParams;
};

} // namespace sso
