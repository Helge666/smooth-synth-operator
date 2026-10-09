#pragma once

namespace sso::kiwi3p
{

/** Roland JX-3P with KiwiTechnics Kiwi-3P upgrade, firmware 4.x.

    Parameter offsets, CC numbers and value mappings follow the v4.00 manual, corrected
    by measurements against real hardware (2026-10-09, see docs/kiwi3p-midi-notes.md).
*/
class Kiwi3PDevice final : public DeviceDefinition
{
public:
    Kiwi3PDevice();

    static constexpr int patchSize = 128;
    static constexpr int nameLength = 20;
    static constexpr int numSlots = 256;

    juce::String getDeviceId() const override     { return "kiwi3p"; }
    juce::String getDisplayName() const override  { return "Roland JX-3P / Kiwi-3P"; }
    juce::String getLibraryFolderName() const override { return "Kiwi-3P"; }
    int getPatchSize() const override             { return patchSize; }
    int getMaxNameLength() const override         { return nameLength; }
    int getNumSlots() const override              { return numSlots; }
    juce::String getSlotName (int slot) const override;

    Patch createInitPatch() const override;
    juce::String getPatchName (const Patch&) const override;
    void setPatchName (Patch&, const juce::String&) const override;

    std::vector<juce::MidiMessage> createParamMessages (const MidiSettings&, int paramIndex,
                                                        const std::function<int (int)>& valueOf) const override;
    juce::MidiMessage createEditBufferRequest (const MidiSettings&) const override;
    juce::MidiMessage createEditBufferDump (const MidiSettings&, const Patch&) const override;
    juce::MidiMessage createSlotRequest (const MidiSettings&, int slot) const override;
    juce::MidiMessage createSlotDump (const MidiSettings&, const Patch&, int slot) const override;
    std::vector<int> getParamsToRefreshAfterDump() const override;
    std::optional<IncomingPatch> parsePatchDump (const MidiSettings&, const juce::MidiMessage&) const override;

    // SysEx command IDs
    enum Command : uint8_t
    {
        requestGlobals      = 0x01,
        globalsDump         = 0x02,
        requestEditBuffer   = 0x03,
        editBufferDump      = 0x04,
        requestTone         = 0x05,
        toneDump            = 0x06,
        editBufferParameter = 0x0e,
        globalParameter     = 0x10
    };

    static juce::MidiMessage createSysEx (const MidiSettings&, uint8_t command, std::initializer_list<uint8_t> payload,
                                          const Patch* patch = nullptr);
};

} // namespace sso::kiwi3p
