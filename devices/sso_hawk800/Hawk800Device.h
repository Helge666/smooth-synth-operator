#pragma once

namespace sso::hawk800
{

/** Korg Poly-800 (MK1) with the HAWK-800 upgrade, software v2.68.

    Patch = 128 bytes, one per parameter in the synth's numbering: bytes 0-63 are the original
    parameters P1 11-88, bytes 64-127 the HAWK's extended parameters P2 11-88. Values are stored
    0-based (parameters the manual counts from 1 are stored as value - 1). Measured against real
    hardware 2026-10-12, see docs/hawk800-midi-notes.md.

    The synth keeps no names: 32 more bytes hold the name in the app only and are never sent.
    It stores 4 banks of 64 sounds and addresses them within the selected bank.
*/
class Hawk800Device final : public DeviceDefinition
{
public:
    Hawk800Device();

    static constexpr int synthPatchSize = 128;
    static constexpr int nameLength = 32;
    static constexpr int numBanks = 4;
    static constexpr int slotsPerBank = 64;
    static constexpr int editBufferSlot = 64;       // "patch 64" in the SysEx = the running sound

    juce::String getDeviceId() const override           { return "hawk800"; }
    juce::String getDisplayName() const override        { return "Korg Poly-800 / HAWK-800"; }
    juce::String getLibraryFolderName() const override  { return "Hawk-800"; }
    int getPatchSize() const override                   { return synthPatchSize + nameLength; }
    int getMaxNameLength() const override               { return nameLength; }
    int getNumSlots() const override                    { return numBanks * slotsPerBank; }
    juce::String getSlotName (int slot) const override;

    SlotLayout getSlotLayout() const override           { return { numBanks, 8, 8, 2 }; }
    juce::String getBankName (int bank) const override  { return juce::String (bank + 1); }
    juce::String getGroupName (int group) const override { return "-" + juce::String (group + 1) + "x"; }

    Patch createInitPatch() const override;
    juce::String getPatchName (const Patch&) const override;
    void setPatchName (Patch&, const juce::String&) const override;
    bool storesPatchNames() const override              { return false; }
    Patch asStored (const Patch&) const override;

    std::vector<juce::MidiMessage> createParamMessages (const MidiSettings&, int paramIndex,
                                                        const std::function<int (int)>& valueOf) const override;
    juce::MidiMessage createEditBufferRequest (const MidiSettings&) const override;
    juce::MidiMessage createEditBufferDump (const MidiSettings&, const Patch&) const override;
    juce::MidiMessage createSlotRequest (const MidiSettings&, int slot) const override;
    juce::MidiMessage createSlotDump (const MidiSettings&, const Patch&, int slot) const override;
    std::optional<IncomingPatch> parsePatchDump (const MidiSettings&, const juce::MidiMessage&) const override;
    std::vector<IncomingPatch> parseBulkDump (const MidiSettings&, const juce::MidiMessage&) const override;

    int getSlotBank (int slot) const override           { return slot / slotsPerBank; }
    juce::MidiMessage createBankSelect (const MidiSettings&, int bank) const override;
    int getSelectedBank (const std::vector<uint8_t>& globalDump) const override;

    juce::MidiMessage createGlobalsRequest (const MidiSettings&) const override;
    std::optional<std::vector<uint8_t>> parseGlobalsDump (const MidiSettings&, const juce::MidiMessage&) const override;

    // SysEx: F0 42 21 <command> ... F7 (Korg, Poly-800). Data bytes travel as high/low nibbles.
    enum Command : uint8_t
    {
        patchDump        = 0x06,    // + patch number (0-63, 64 = edit buffer) + 256 nibbles; ack 7E 06 nn
        requestPatch     = 0x07,
        globalsDump      = 0x08,
        bulkRequest      = 0x0a,    // + 0 patches, 1 sequences, 2 globals
        bankChange       = 0x0e,    // + bank 0-3; ack 7E 0E 01
        parameterChange  = 0x0f,    // + parameter 0-127 + stored value
        flashUpload      = 0x7f,    // 00 00-03 software and boot images, 00 04-07 a whole patch bank (bank 1-4)
    };

    static juce::MidiMessage createSysEx (std::initializer_list<uint8_t> header, const uint8_t* nibbleData = nullptr, int numBytes = 0);
};

} // namespace sso::hawk800
