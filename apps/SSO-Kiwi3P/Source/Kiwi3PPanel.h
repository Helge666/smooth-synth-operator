#pragma once

#include <sso_kiwi3p/sso_kiwi3p.h>

/** The Kiwi-3P console: two rows of sections, modulation matrix, voice and generator. */
class Kiwi3PPanel final : public juce::Component
{
public:
    Kiwi3PPanel (sso::SynthController&, juce::AudioProcessorValueTreeState&, sso::PatchGenerator&);
    ~Kiwi3PPanel() override;

    void resized() override;

    static constexpr int rowHeight = 280;

private:
    void layoutRow (std::vector<juce::Component*> row, std::vector<float> grows, std::vector<int> preferred,
                    juce::Rectangle<int> area);

    std::vector<std::unique_ptr<sso::SectionPanel>> row1, row2;
    std::unique_ptr<sso::ModMatrix> matrix;
    std::unique_ptr<sso::SectionPanel> voice;
    std::unique_ptr<sso::GeneratorPanel> generator;
};
