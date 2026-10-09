namespace sso
{

static constexpr int controlWidth = 78;
static constexpr int controlHeight = 92;
static constexpr int sectionHeader = 24;
static constexpr int sectionPad = 8;

struct GenericParamPanel::ParamControl : public juce::Component
{
    ParamControl (const ParamSpec& s, juce::AudioProcessorValueTreeState& state) : spec (s)
    {
        label.setText (spec.name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (label);

        if (spec.kind == ParamSpec::Kind::choice)
        {
            combo.addItemList (spec.choices, 1);
            addAndMakeVisible (combo);
            comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, spec.id, combo);
        }
        else if (spec.kind == ParamSpec::Kind::toggle)
        {
            toggle.setButtonText ({});
            addAndMakeVisible (toggle);
            buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, spec.id, toggle);
        }
        else
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, controlWidth - 8, 18);
            if (spec.kind == ParamSpec::Kind::bipolar)
                slider.setDoubleClickReturnValue (true, spec.centreValue);
            addAndMakeVisible (slider);
            sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, spec.id, slider);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2);
        label.setBounds (r.removeFromTop (16));

        if (spec.kind == ParamSpec::Kind::choice)
            combo.setBounds (r.withSizeKeepingCentre (r.getWidth(), 24));
        else if (spec.kind == ParamSpec::Kind::toggle)
            toggle.setBounds (r.withSizeKeepingCentre (28, 28));
        else
            slider.setBounds (r);
    }

    const ParamSpec& spec;
    juce::Label label;
    juce::Slider slider;
    juce::ComboBox combo;
    juce::ToggleButton toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
};

struct GenericParamPanel::Section : public juce::Component
{
    explicit Section (const juce::String& n) : name (n) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (juce::Colour (0xff35353a));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colour (0xffe8e2d0));
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (name.toUpperCase(), r.removeFromTop ((float) sectionHeader).reduced (8.0f, 0.0f),
                    juce::Justification::centredLeft);
    }

    int columnsFor (int width) const
    {
        return juce::jmax (1, (width - 2 * sectionPad) / controlWidth);
    }

    int heightFor (int width) const
    {
        auto cols = columnsFor (width);
        auto rows = ((int) controls.size() + cols - 1) / cols;
        return sectionHeader + rows * controlHeight + sectionPad;
    }

    void resized() override
    {
        auto cols = columnsFor (getWidth());

        for (size_t i = 0; i < controls.size(); ++i)
        {
            auto col = (int) i % cols, row = (int) i / cols;
            controls[i]->setBounds (sectionPad + col * controlWidth, sectionHeader + row * controlHeight,
                                    controlWidth, controlHeight);
        }
    }

    juce::String name;
    std::vector<std::unique_ptr<ParamControl>> controls;
};

GenericParamPanel::GenericParamPanel (const DeviceDefinition& device, juce::AudioProcessorValueTreeState& state)
{
    for (auto& sectionName : device.getSections())
    {
        auto section = std::make_unique<Section> (sectionName);

        for (auto& spec : device.getParams())
        {
            if (spec.section != sectionName)
                continue;

            auto control = std::make_unique<ParamControl> (spec, state);
            section->addAndMakeVisible (*control);
            section->controls.push_back (std::move (control));
        }

        addAndMakeVisible (*section);
        sections.push_back (std::move (section));
    }
}

GenericParamPanel::~GenericParamPanel() = default;

int GenericParamPanel::layoutSections (int width, bool apply) const
{
    // Sections flow left to right; a section takes the width its controls need
    // (max. 8 per row) and wraps to the next line when the row is full.
    int x = 0, y = 0, rowHeight = 0;

    for (auto& s : sections)
    {
        auto cols = juce::jmin (8, (int) s->controls.size());
        auto w = juce::jmin (width, cols * controlWidth + 2 * sectionPad);

        if (x > 0 && x + w > width)
        {
            x = 0;
            y += rowHeight;
            rowHeight = 0;
        }

        auto h = s->heightFor (w);

        if (apply)
            s->setBounds (x, y, w, h);

        x += w;
        rowHeight = juce::jmax (rowHeight, h);
    }

    return y + rowHeight;
}

int GenericParamPanel::getRequiredHeight (int width) const
{
    return layoutSections (width, false);
}

void GenericParamPanel::resized()
{
    layoutSections (getWidth(), true);
}

} // namespace sso
