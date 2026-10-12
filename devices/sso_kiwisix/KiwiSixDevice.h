#pragma once

namespace sso::kiwisix
{

/** Korg PolySix with KiwiTechnics KiwiSix upgrade, firmware v5.00.

    Layout, CC map and value mappings follow the v5.0 manual, corrected by measurements
    against real hardware (2026-10-11, see docs/kiwisix-midi-notes.md). Continuous controls
    are 12 bit and sent with SysEx (one exact message); switches use CC.
*/
class KiwiSixDevice final : public kiwitechnics::KiwiTechnicsDevice
{
public:
    KiwiSixDevice();

    static constexpr int numSlots = 512;
    static constexpr int maxValue12 = 4095;

    juce::String getDeviceId() const override     { return "kiwisix"; }
    juce::String getDisplayName() const override  { return "Korg PolySix / KiwiSix"; }
    juce::String getLibraryFolderName() const override { return "KiwiSix"; }
    int getNumSlots() const override              { return numSlots; }
    juce::String getSlotName (int slot) const override;

    SlotLayout getSlotLayout() const override     { return { 8, 8, 8, 2 }; }
    juce::String getBankName (int bank) const override;

    Patch createInitPatch() const override;
    std::optional<TuneSound> getTuneSound() const override;
    Patch asStored (const Patch&) const override;

    std::vector<juce::MidiMessage> createParamMessages (const MidiSettings&, int paramIndex,
                                                        const std::function<int (int)>& valueOf) const override;
};

} // namespace sso::kiwisix
