#pragma once

namespace sso::kiwitechnics
{

/** What KiwiTechnics upgrades (Kiwi-3P, KiwiSix, ...) have in common:

    SysEx `F0 00 21 16 60 <model> <dev> <cmd> ... F7`, 128-byte tones whose first 20 bytes
    are the ASCII name, edit-buffer and stored-tone dumps addressed by bank + tone.
*/
class KiwiTechnicsDevice : public DeviceDefinition
{
public:
    explicit KiwiTechnicsDevice (uint8_t modelId);

    static constexpr int patchSize = 128;
    static constexpr int nameLength = 20;

    int getPatchSize() const override             { return patchSize; }
    int getMaxNameLength() const override         { return nameLength; }

    juce::String getPatchName (const Patch&) const override;
    void setPatchName (Patch&, const juce::String&) const override;

    juce::MidiMessage createEditBufferRequest (const MidiSettings&) const override;
    juce::MidiMessage createEditBufferDump (const MidiSettings&, const Patch&) const override;
    juce::MidiMessage createSlotRequest (const MidiSettings&, int slot) const override;
    juce::MidiMessage createSlotDump (const MidiSettings&, const Patch&, int slot) const override;
    std::optional<IncomingPatch> parsePatchDump (const MidiSettings&, const juce::MidiMessage&) const override;

    juce::MidiMessage createGlobalsRequest (const MidiSettings&) const override;
    std::optional<std::vector<uint8_t>> parseGlobalsDump (const MidiSettings&, const juce::MidiMessage&) const override;
    std::vector<juce::MidiMessage> createGlobalMessages (const MidiSettings&, const std::function<int (int)>&) const override;

    // SysEx command IDs shared by the upgrades
    enum Command : uint8_t
    {
        requestGlobals      = 0x01,
        globalsDump         = 0x02,
        requestEditBuffer   = 0x03,
        editBufferDump      = 0x04,
        requestTone         = 0x05,
        toneDump            = 0x06,
        editBufferParameter = 0x0e,
        globalParameter     = 0x10,     // RAM only - the synths store globals only from their panel
    };

    juce::MidiMessage createSysEx (const MidiSettings&, uint8_t command, std::initializer_list<uint8_t> payload,
                                   const Patch* patch = nullptr) const;

    uint8_t getModelId() const noexcept   { return modelId; }

private:
    const uint8_t modelId;
};

} // namespace sso::kiwitechnics
