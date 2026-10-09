#pragma once

namespace sso
{

/** The synth's stored sounds as a bank selector plus 16 tiles.
    - click a tile: load that sound into the editor
    - drop a library preset on a tile: write it to that memory (after confirmation,
      verified by reading it back)
    - READ ALL: refresh the cached view of the synth's memory
*/
class SynthMemoryPanel : public juce::Component,
                         public juce::DragAndDropTarget,
                         public juce::SettableTooltipClient
{
public:
    SynthMemoryPanel (PresetLibrary&, SynthController&);
    ~SynthMemoryPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    static constexpr const char* dragPrefix = "sso-preset:";

private:
    int slotsPerBank() const noexcept { return 16; }
    int numBanks() const;
    int slotAt (juce::Point<int>) const;
    juce::Rectangle<int> tileBounds (int indexInBank) const;
    juce::Rectangle<int> bankButtonBounds (int bank) const;

    void loadSlotIntoEditor (int slot);
    void writePresetToSlot (const Preset&, int slot);
    void readAll();
    void importBank();
    void setStatus (const juce::String&);

    juce::File cacheFile() const;
    void loadCache();
    void saveCache() const;

    PresetLibrary& library;
    SynthController& controller;
    BankReader reader;
    std::vector<std::optional<Patch>> cache;
    juce::Time cacheTime;

    int bank = 0;
    int hoverSlot = -1;
    juce::String status;
    juce::TextButton readButton { "READ ALL" }, importButton { "IMPORT BANK" };
    juce::ScopedMessageBox messageBox;

    static constexpr int headerHeight = 30;
    static constexpr int bankRowHeight = 26;
    static constexpr int tileHeight = 42;
};

} // namespace sso
