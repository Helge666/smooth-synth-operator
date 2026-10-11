#pragma once

#include <sso_kiwisix/sso_kiwisix.h>

/** The PolySix console: VCO, VCF, VCA, EG in the first row; MG, effects, key assign /
    arpeggiator and the generator in the second. Knobs like the original panel. */
class KiwiSixPanel final : public juce::Component
{
public:
    KiwiSixPanel (sso::SynthController&, juce::AudioProcessorValueTreeState&, sso::PatchGenerator&);
    ~KiwiSixPanel() override;

    void resized() override;

private:
    static void layoutRow (std::vector<juce::Component*> row, std::vector<float> grows, std::vector<int> preferred,
                           juce::Rectangle<int> area);

    std::vector<std::unique_ptr<sso::SectionPanel>> row1, row2;
    std::unique_ptr<sso::GeneratorPanel> generator;
};
