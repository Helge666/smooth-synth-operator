#pragma once

namespace sso
{

/** Sidebar: library search/filter/list on top, synth memory below. */
class PresetBrowser : public juce::Component,
                      public juce::DragAndDropContainer,
                      private juce::ChangeListener,
                      private juce::ListBoxModel
{
public:
    PresetBrowser (PresetLibrary&, SynthController&);
    ~PresetBrowser() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Opens the "save as" dialog (also used by the editor's rename). */
    void saveAs();
    void renameCurrent();

private:
    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>&) override;
    juce::String getTooltipForRow (int row) override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void rebuildFilterButtons();
    void refreshResults();
    const Preset* presetForRow (int row) const;
    void loadPreset (const Preset&);
    void showPresetMenu (const Preset&);
    void saveCurrent();
    void importSyx();
    void askForText (const juce::String& title, const juce::String& message, const juce::String& initial,
                     std::function<void (juce::String)> onOk);

    PresetLibrary& library;
    SynthController& controller;

    juce::TextEditor search;
    juce::OwnedArray<juce::TextButton> bankButtons, categoryButtons;
    juce::ListBox list;
    juce::TextButton saveButton { "SAVE" }, saveAsButton { "SAVE AS..." }, importButton { "IMPORT .SYX" };
    SynthMemoryPanel memory;

    PresetLibrary::Filter filter;
    std::vector<int> results;

    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AlertWindow> dialog;
    juce::ScopedMessageBox messageBox;

    static constexpr int memoryHeight = 330;
    static constexpr int rowHeight = 30;
};

} // namespace sso
