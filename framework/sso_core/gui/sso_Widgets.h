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
    KnobStrip (const ParamSpec&, juce::AudioProcessorValueTreeState&, const juce::String& label = {}, int knobSize = 64);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int defaultKnobSize = 64;
    static constexpr int spacing = 6;
    static constexpr int textHeight = 38;      // name and value below the knob

    static int widthFor (int knobSize) noexcept     { return knobSize + 6; }

private:
    const ParamSpec& spec;
    const int knobSize;
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
                   const juce::String& label = {}, const juce::StringArray& shortNames = {}, int maxColumns = 0,
                   std::vector<int> flags = {});

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
    std::vector<int> flags; // value -> buttons lit (bit i = button i); empty = one button per value
    int current = 0;

    bool isLit (int button) const;
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
/** Envelope shape drawn from its parameters, LCD style: attack, decay, sustain, release - or
    attack, decay, break point, slope, sustain, release (Korg style: decay falls to the break
    point level, slope then moves to the sustain level). */
class EnvelopeGraph : public juce::Component,
                      private juce::Timer
{
public:
    EnvelopeGraph (const SynthController&, std::vector<int> paramIndices);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    const SynthController& controller;
    std::vector<int> indices;
    std::vector<int> maxValues;
    std::vector<int> shown;
};

//==============================================================================
/** Describes one panel section: a coloured frame with faders and selectors. */
struct SectionDef
{
    struct Fader    { juce::String id, label; };
    struct Selector
    {
        juce::String id, label;
        juce::StringArray shortNames;
        int columns = 0;
        std::vector<int> flags;     // optional: shortNames are switches, flags[value] says which are on
    };
    struct Toggles  { juce::String label; juce::StringArray ids, names; };

    juce::String title;
    juce::Colour accent;
    float grow = 1.0f;              // share of extra width beyond the preferred width
    int selectorWidth = 120;
    std::vector<Fader> faders;
    std::vector<Selector> selectors;
    std::vector<juce::String> envelope;     // ADSR (or A D BP SL S R) parameter ids for a graph, optional
    std::vector<Toggles> toggles;           // groups of on/off switches below the selectors
    bool knobs = false;                     // rotary knobs instead of faders
    int knobSize = KnobStrip::defaultKnobSize;
    int knobColumns = 0;                    // knobs per row, 0 = all in one row
    int graphHeight = 64;
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
    int stripsPerRow() const noexcept;

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
