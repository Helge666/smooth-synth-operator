#pragma once

namespace sso
{

/** Modulation sources (rows) x destinations (columns). Each cell is bound to a
    routing parameter: click cycles through its values, right-click switches it off. */
struct ModMatrixDef
{
    struct Row
    {
        juce::String source;
        juce::String depthId;               // optional bipolar depth parameter
        std::vector<juce::String> cells;    // parameter id per destination, empty = not available
    };

    juce::String title;
    juce::Colour accent;
    juce::StringArray destinations;
    std::vector<Row> rows;
};

class ModMatrix : public juce::Component
{
public:
    ModMatrix (const ModMatrixDef&, SynthController&, juce::AudioProcessorValueTreeState&);
    ~ModMatrix() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Cell;
    struct DepthSlider;

    ModMatrixDef def;
    std::vector<std::unique_ptr<Cell>> cells;
    std::vector<std::unique_ptr<DepthSlider>> depths;

    static constexpr int sourceWidth = 96;
    static constexpr int depthWidth = 120;
};

} // namespace sso
