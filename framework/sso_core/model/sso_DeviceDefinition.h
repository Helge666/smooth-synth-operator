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

protected:
    std::vector<ParamSpec> params;
};

} // namespace sso
