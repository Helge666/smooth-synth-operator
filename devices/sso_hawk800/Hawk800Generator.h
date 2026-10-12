#pragma once

namespace sso::hawk800
{

/** Patch generator with the PG-2K style categories, tuned on the original Poly-800 sounds. */
std::unique_ptr<PatchGenerator> createGenerator (const Hawk800Device&);

/** Name of the original Korg sound at program 11-88 (slot 0-63), as in factory bank 2. */
juce::String getKorgName (int slotInBank);

/** Imports the HAWK-800 factory banks (Bank1.syx ... Bank4.syx from hawk800.com, in `folder`):
    the original Korg sounds (factory bank 2, with their Korg names) into the library bank
    `korgBank`, everything else plus an "Init" sound into `hawkBank`. HAWK variations of a Korg
    sound are named "<name> A", "B", ..., all others "Sound <bank>-<number>". */
PresetLibrary::ImportResult importFactoryBanks (PresetLibrary&, const Hawk800Device&, const juce::File& folder,
                                                const juce::String& hawkBank = "HAWK Factory",
                                                const juce::String& korgBank = "Korg Factory");

} // namespace sso::hawk800
