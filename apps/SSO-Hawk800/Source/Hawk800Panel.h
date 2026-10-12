#pragma once

#include <sso_hawk800/sso_hawk800.h>

/** The Poly-800 console (draft B "Black Panel"): DCO 1, DCO 2, noise and VCF in the first row,
    the three envelopes and velocity in the second, the HAWK's modulation pages (LFOs, routing,
    playing) and the generator in the third. */
class Hawk800Panel final : public juce::Component
{
public:
    Hawk800Panel (sso::SynthController&, juce::AudioProcessorValueTreeState&, sso::PatchGenerator&);
    ~Hawk800Panel() override;

    void resized() override;

private:
    void showPage (int);

    std::vector<std::unique_ptr<sso::SectionPanel>> row1, row2;
    std::vector<std::vector<std::unique_ptr<sso::SectionPanel>>> pages;
    juce::OwnedArray<juce::TextButton> pageButtons;
    int currentPage = 0;
    std::unique_ptr<sso::GeneratorPanel> generator;
    juce::SharedResourcePointer<sso::AppSettings> settings;

    static constexpr int pageTabHeight = 28;
};
