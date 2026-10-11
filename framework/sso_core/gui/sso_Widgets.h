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
/** Rotary knob with name and value below, bound to a parameter. */
class KnobStrip : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    KnobStrip (const ParamSpec&, juce::AudioProcessorValueTreeState&, const juce::String& label = {});

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int preferredWidth = 70;
    static constexpr int knobSize = 64;
    static constexpr int spacing = 6;

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
                   const juce::String& label = {}, const juce::StringArray& shortNames = {}, int maxColumns = 0);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    int getPreferredHeight (int width) const;

private:
    juce::Rectangle<int> getButtonBounds (int index, int width) const;
    int getColumns (int width) const;

    const ParamSpec& spec;
    juce::String label;
    juce::StringArray names;
    int maxColumns = 0;     // 0 = as many as fit
    int current = 0;
    juce::ParameterAttachment attachment;
};

//==============================================================================
/** Buttons with LEDs, each switching its own on/off parameter (e.g. modulation destinations). */
class ToggleGroup : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    ToggleGroup (const std::vector<const ParamSpec*>&, juce::AudioProcessorValueTreeState&,
                 const juce::String& label, const juce::StringArray& names);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    int getPreferredHeight (int width) const;

private:
    juce::Rectangle<int> getButtonBounds (int index, int width) const;

    juce::String label;
    juce::StringArray names;
    std::vector<int> states;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> attachments;
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
    std::array<int, 4> maxValues { 127, 127, 127, 127 };
    std::array<int, 4> shown { -1, -1, -1, -1 };
};

//==============================================================================
/** Describes one panel section: a coloured frame with faders and selectors. */
struct SectionDef
{
    struct Fader    { juce::String id, label; };
    struct Selector { juce::String id, label; juce::StringArray shortNames; int columns = 0; };
    struct Toggles  { juce::String label; juce::StringArray ids, names; };

    juce::String title;
    juce::Colour accent;
    float grow = 1.0f;              // share of extra width beyond the preferred width
    int selectorWidth = 120;
    std::vector<Fader> faders;
    std::vector<Selector> selectors;
    std::array<juce::String, 4> envelope;   // ADSR parameter ids for a graph, optional
    std::vector<Toggles> toggles;           // groups of on/off switches below the selectors
    bool knobs = false;                     // rotary knobs instead of faders
    juce::String subtitle;                  // small text next to the title (tabbed style)
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
    int stripWidth() const noexcept;

    SectionDef def;
    std::vector<std::unique_ptr<juce::Component>> faders;     // FaderStrip or KnobStrip
    std::vector<std::unique_ptr<ChoiceButtons>> selectors;
    std::vector<std::unique_ptr<ToggleGroup>> toggles;
    std::unique_ptr<EnvelopeGraph> graph;
};

/** Draws the frame and title used by all sections: a coloured stripe on top or, with
    theme::tabbedSections, an outline with a title tab. */
void paintSectionFrame (juce::Graphics&, juce::Rectangle<int> bounds, const juce::String& title, juce::Colour accent,
                        const juce::String& subtitle = {});

} // namespace sso
