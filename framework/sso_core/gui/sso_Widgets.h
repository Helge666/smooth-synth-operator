#pragma once

namespace sso
{

/** Vertical fader with name and value below, bound to a parameter. */
class FaderStrip : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    FaderStrip (const ParamSpec&, juce::AudioProcessorValueTreeState&, const juce::String& label = {});

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int preferredWidth = 38;
    static constexpr int spacing = 4;

private:
    const ParamSpec& spec;
    juce::String label;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
/** Row of small buttons with LEDs selecting one value of a choice/toggle parameter. */
class ChoiceButtons : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    ChoiceButtons (const ParamSpec&, juce::AudioProcessorValueTreeState&,
                   const juce::String& label = {}, const juce::StringArray& shortNames = {});

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    int getPreferredHeight (int width) const;

private:
    juce::Rectangle<int> getButtonBounds (int index, int width) const;
    int getColumns (int width) const;

    const ParamSpec& spec;
    juce::String label;
    juce::StringArray names;
    int current = 0;
    juce::ParameterAttachment attachment;
};

//==============================================================================
/** ADSR shape drawn from four parameters, LCD style. */
class EnvelopeGraph : public juce::Component,
                      private juce::Timer
{
public:
    EnvelopeGraph (const SynthController&, std::array<int, 4> adsrParamIndices);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    const SynthController& controller;
    std::array<int, 4> indices;
    std::array<int, 4> shown { -1, -1, -1, -1 };
};

//==============================================================================
/** Describes one panel section: a coloured frame with faders and selectors. */
struct SectionDef
{
    struct Fader    { juce::String id, label; };
    struct Selector { juce::String id, label; juce::StringArray shortNames; };

    juce::String title;
    juce::Colour accent;
    float grow = 1.0f;              // share of extra width beyond the preferred width
    int selectorWidth = 120;
    std::vector<Fader> faders;
    std::vector<Selector> selectors;
    std::array<juce::String, 4> envelope;   // ADSR parameter ids for a graph, optional
};

class SectionPanel : public juce::Component
{
public:
    SectionPanel (const SectionDef&, SynthController&, juce::AudioProcessorValueTreeState&);

    void paint (juce::Graphics&) override;
    void resized() override;

    const SectionDef& getDef() const noexcept { return def; }
    int getPreferredWidth() const;

    static constexpr int stripeHeight = 6;
    static constexpr int titleHeight = 30;

private:
    SectionDef def;
    std::vector<std::unique_ptr<FaderStrip>> faders;
    std::vector<std::unique_ptr<ChoiceButtons>> selectors;
    std::unique_ptr<EnvelopeGraph> graph;
};

/** Draws the coloured stripe, background and title used by all sections. */
void paintSectionFrame (juce::Graphics&, juce::Rectangle<int> bounds, const juce::String& title, juce::Colour accent);

} // namespace sso
