#pragma once

namespace sso::kiwi3p
{

/** Roland JX-3P with KiwiTechnics Kiwi-3P upgrade, firmware 4.x.

    Parameter offsets, CC numbers and value mappings follow the v4.00 manual, corrected
    by measurements against real hardware (2026-10-09, see docs/kiwi3p-midi-notes.md).
*/
class Kiwi3PDevice final : public kiwitechnics::KiwiTechnicsDevice
{
public:
    Kiwi3PDevice();

    static constexpr int numSlots = 256;

    juce::String getDeviceId() const override     { return "kiwi3p"; }
    juce::String getDisplayName() const override  { return "Roland JX-3P / Kiwi-3P"; }
    juce::String getLibraryFolderName() const override { return "Kiwi-3P"; }
    int getNumSlots() const override              { return numSlots; }
    juce::String getSlotName (int slot) const override;

    Patch createInitPatch() const override;
    std::optional<TuneSound> getTuneSound() const override;

    std::vector<juce::MidiMessage> createParamMessages (const MidiSettings&, int paramIndex,
                                                        const std::function<int (int)>& valueOf) const override;
    std::vector<int> getParamsToRefreshAfterDump() const override;
};

} // namespace sso::kiwi3p
