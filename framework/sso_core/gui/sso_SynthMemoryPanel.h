#pragma once

namespace sso
{

/** The synth's stored sounds: bank selector, optional group selector (e.g. KiwiSix 1-8 within
    a bank, like the synth's first tone button) and one tile per sound.
    - click a tile: load that sound into the editor
    - drop a library preset on a tile: write it to that memory (after confirmation,
      verified by reading it back)
    - READ ALL: refresh the cached view of the synth's memory
    Folded to its title bar unless needed (click the bar, or drag a sound onto it).
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

    /** Height that fits the selectors, one group of tiles, status line and buttons -
        or just the title bar while folded. */
    int getPreferredHeight() const;

    bool isOpen() const noexcept    { return open; }
    void setOpen (bool);
    std::function<void()> onOpenChanged;    // the owner re-lays out

private:
    int slotsPerBank() const noexcept   { return layout.groupsPerBank * layout.slotsPerGroup; }
    int firstShownSlot() const noexcept { return bank * slotsPerBank() + group * layout.slotsPerGroup; }
    int slotAt (juce::Point<int>) const;
    int tilesTop() const noexcept;
    juce::Rectangle<int> tileBounds (int indexInGroup) const;
    juce::Rectangle<int> bankButtonBounds (int bank) const;
    juce::Rectangle<int> groupButtonBounds (int group) const;
    void verifyWrite (const Preset&, int slot, int attempt);

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

    const DeviceDefinition::SlotLayout layout;
    int bank = 0, group = 0;
    int hoverSlot = -1;
    juce::String status;
    juce::TextButton readButton { "READ ALL" }, importButton { "IMPORT BANK" }, foldButton;
    juce::ScopedMessageBox messageBox;
    juce::SharedResourcePointer<AppSettings> settings;
    bool open = false;

    static constexpr int headerHeight = 30;
    static constexpr int bankRowHeight = 26;
    static constexpr int tileHeight = 42;
};

} // namespace sso
