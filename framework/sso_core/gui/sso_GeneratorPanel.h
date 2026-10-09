#pragma once

namespace sso
{

/** Category buttons, keep (mask) toggles, variation and a back step for the patch generator. */
class GeneratorPanel : public juce::Component
{
public:
    GeneratorPanel (PatchGenerator&, SynthController&);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr const char* generatedBank = "Generated";

private:
    juce::StringArray keptGroups() const;
    void generate (const juce::String& category);
    void varyCurrent();
    void goBack();
    void apply (const Patch&, const juce::String& category);

    PatchGenerator& generator;
    SynthController& controller;
    juce::Random random;

    juce::OwnedArray<juce::TextButton> categoryButtons, keepButtons;
    juce::TextButton initButton { "INIT" }, variationButton { "VARIATION" }, backButton { "BACK" };
    juce::Slider amount;

    struct HistoryEntry { Patch patch; SynthController::PresetRef ref; };
    std::vector<HistoryEntry> history;
    static constexpr size_t maxHistory = 20;
};

} // namespace sso
