namespace sso
{

struct ModMatrix::Cell : public juce::Component,
                         public juce::SettableTooltipClient
{
    Cell (const ParamSpec* s, juce::AudioProcessorValueTreeState& state, const juce::String& tip) : spec (s)
    {
        if (spec != nullptr)
        {
            attachment = std::make_unique<juce::ParameterAttachment> (*state.getParameter (spec->id),
                                                                       [this] (float v) { value = juce::roundToInt (v); repaint(); });
            attachment->sendInitialUpdate();
            setTooltip (tip + "  (click: next, right-click: off)");
        }
    }

    juce::String text() const
    {
        if (spec == nullptr)
            return {};

        if (value == 0)
            return juce::String::charToString (0x00b7);   // middle dot

        if (spec->kind == ParamSpec::Kind::toggle)
            return "ON";

        // Off / X1 / X2 / Both
        return value == 3 ? "1+2" : juce::String (value);
    }

    void paint (juce::Graphics& g) override
    {
        if (spec == nullptr)
            return;

        auto on = value != 0;
        g.setColour (on ? theme::yellow : theme::control);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
        g.setColour (on ? theme::background : theme::textOff);
        g.setFont (theme::label (13.0f, true));
        g.drawText (text(), getLocalBounds(), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (attachment == nullptr)
            return;

        auto next = e.mods.isPopupMenu() ? 0 : (value + 1) % (spec->maxValue - spec->minValue + 1);
        attachment->setValueAsCompleteGesture ((float) next);
    }

    const ParamSpec* spec;
    int value = 0;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

struct ModMatrix::DepthSlider : public juce::Component
{
    DepthSlider (const ParamSpec& s, juce::AudioProcessorValueTreeState& state) : spec (s)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setSliderSnapsToMousePosition (false);
        slider.setDoubleClickReturnValue (true, spec.defaultValue);
        slider.getProperties().set ("bipolar", spec.kind == ParamSpec::Kind::bipolar);
        slider.onValueChange = [this] { repaint(); };
        slider.setTooltip (spec.section + " " + spec.name);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, spec.id, slider);
    }

    void resized() override { slider.setBounds (getLocalBounds().withTrimmedRight (34)); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::textDim);
        g.setFont (theme::label (13.0f));
        g.drawText (spec.toText (juce::roundToInt (slider.getValue())), getLocalBounds().removeFromRight (32),
                    juce::Justification::centredRight);
    }

    const ParamSpec& spec;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

ModMatrix::ModMatrix (const ModMatrixDef& d, SynthController& controller, juce::AudioProcessorValueTreeState& state)
    : def (d)
{
    auto& device = controller.getDevice();

    for (auto& row : def.rows)
    {
        if (auto i = device.indexOf (row.depthId); i >= 0)
        {
            depths.push_back (std::make_unique<DepthSlider> (device.getParams()[(size_t) i], state));
            addAndMakeVisible (*depths.back());
        }
        else
        {
            depths.push_back (nullptr);
        }

        for (int c = 0; c < def.destinations.size(); ++c)
        {
            auto id = c < (int) row.cells.size() ? row.cells[(size_t) c] : juce::String();
            auto i = device.indexOf (id);
            cells.push_back (std::make_unique<Cell> (i >= 0 ? &device.getParams()[(size_t) i] : nullptr, state,
                                                     row.source + " > " + def.destinations[c]));
            addAndMakeVisible (*cells.back());
        }
    }
}

ModMatrix::~ModMatrix() = default;

void ModMatrix::paint (juce::Graphics& g)
{
    paintSectionFrame (g, getLocalBounds(), def.title, def.accent);

    auto r = getLocalBounds().withTrimmedTop (SectionPanel::titleHeight).reduced (12, 0);
    auto header = r.removeFromTop (20);
    header.removeFromLeft (sourceWidth + depthWidth + 8);
    auto cellWidth = (float) header.getWidth() / (float) def.destinations.size();

    g.setFont (theme::label (11.0f));
    g.setColour (theme::textDim);
    g.drawText ("DEPTH", r.getX() + sourceWidth, header.getY(), depthWidth, 20, juce::Justification::centredLeft);

    for (int c = 0; c < def.destinations.size(); ++c)
        g.drawFittedText (def.destinations[c], juce::Rectangle<float> ((float) header.getX() + (float) c * cellWidth, (float) header.getY(),
                                                                       cellWidth, 20.0f).toNearestInt(),
                          juce::Justification::centred, 1, 0.6f);

    g.setColour (theme::text);
    g.setFont (theme::label (14.0f, true));

    for (size_t row = 0; row < def.rows.size(); ++row)
    {
        auto y = r.getY() + (int) row * 34;
        g.drawText (def.rows[row].source, r.getX(), y, sourceWidth, 28, juce::Justification::centredLeft);
    }
}

void ModMatrix::resized()
{
    auto r = getLocalBounds().withTrimmedTop (SectionPanel::titleHeight + 20).reduced (12, 0);
    auto cellsX = r.getX() + sourceWidth + depthWidth + 8;
    auto cellWidth = (float) (r.getRight() - cellsX) / (float) def.destinations.size();
    auto columns = def.destinations.size();

    for (size_t row = 0; row < def.rows.size(); ++row)
    {
        auto y = r.getY() + (int) row * 34;

        if (depths[row] != nullptr)
            depths[row]->setBounds (r.getX() + sourceWidth, y, depthWidth, 28);

        for (int c = 0; c < columns; ++c)
        {
            auto x = (float) cellsX + (float) c * cellWidth;
            cells[row * (size_t) columns + (size_t) c]->setBounds (juce::Rectangle<float> (x + 1.5f, (float) y, cellWidth - 3.0f, 28.0f).toNearestInt());
        }
    }
}

} // namespace sso
