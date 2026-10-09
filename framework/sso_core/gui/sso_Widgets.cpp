namespace sso
{

void paintSectionFrame (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title, juce::Colour accent)
{
    auto r = bounds.toFloat();
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 6.0f);

    g.saveState();
    g.reduceClipRegion (bounds.withHeight (SectionPanel::stripeHeight));
    g.setColour (accent);
    g.fillRoundedRectangle (r, 6.0f);
    g.restoreState();

    g.setColour (theme::text);
    g.setFont (theme::label (16.0f, true));
    g.drawText (title.toUpperCase(), bounds.withTrimmedTop (SectionPanel::stripeHeight).withHeight (SectionPanel::titleHeight - SectionPanel::stripeHeight)
                                           .withTrimmedLeft (12),
                juce::Justification::centredLeft);
}

//==============================================================================
FaderStrip::FaderStrip (const ParamSpec& s, juce::AudioProcessorValueTreeState& state, const juce::String& l)
    : spec (s), label (l.isEmpty() ? s.name.toUpperCase() : l)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setSliderSnapsToMousePosition (false);
    slider.setDoubleClickReturnValue (true, spec.defaultValue);
    slider.getProperties().set ("bipolar", spec.kind == ParamSpec::Kind::bipolar);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, spec.id, slider);

    setTooltip (spec.section + " " + spec.name + "  (double-click: default)");
    slider.setTooltip (getTooltip());
}

void FaderStrip::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedBottom (36));
}

void FaderStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().removeFromBottom (34);
    g.setColour (theme::text);
    g.setFont (theme::label (13.0f, true));
    g.drawFittedText (label, r.removeFromTop (17), juce::Justification::centred, 1, 0.7f);
    g.setColour (theme::textDim);
    g.setFont (theme::label (13.0f));
    g.drawFittedText (spec.toText (juce::roundToInt (slider.getValue())), r, juce::Justification::centred, 1, 0.7f);
}

//==============================================================================
static constexpr int choiceButtonHeight = 26;
static constexpr int choiceLabelHeight = 16;

ChoiceButtons::ChoiceButtons (const ParamSpec& s, juce::AudioProcessorValueTreeState& state,
                              const juce::String& l, const juce::StringArray& shortNames)
    : spec (s),
      label (l.isEmpty() ? s.name.toUpperCase() : l),
      names (! shortNames.isEmpty() ? shortNames
                                    : (s.kind == ParamSpec::Kind::toggle ? juce::StringArray { "OFF", "ON" } : s.choices)),
      attachment (*state.getParameter (s.id), [this] (float v) { current = juce::roundToInt (v); repaint(); })
{
    attachment.sendInitialUpdate();
    setTooltip (spec.section + " " + spec.name);
}

int ChoiceButtons::getColumns (int width) const
{
    // as many as fit with a minimum button width of 36 px
    return juce::jlimit (1, names.size(), (width + 3) / (36 + 3));
}

int ChoiceButtons::getPreferredHeight (int width) const
{
    auto cols = getColumns (width);
    auto rows = (names.size() + cols - 1) / cols;
    return choiceLabelHeight + rows * (choiceButtonHeight + 3);
}

juce::Rectangle<int> ChoiceButtons::getButtonBounds (int index, int width) const
{
    auto cols = getColumns (width);
    auto rows = (names.size() + cols - 1) / cols;
    auto perRow = (names.size() + rows - 1) / rows;      // balance rows
    auto w = (width - (perRow - 1) * 3) / perRow;
    return { (index % perRow) * (w + 3), choiceLabelHeight + (index / perRow) * (choiceButtonHeight + 3), w, choiceButtonHeight };
}

void ChoiceButtons::paint (juce::Graphics& g)
{
    g.setColour (theme::textDim);
    g.setFont (theme::label (12.0f));
    g.drawText (label, 0, 0, getWidth(), choiceLabelHeight - 2, juce::Justification::bottomLeft);

    for (int i = 0; i < names.size(); ++i)
    {
        auto b = getButtonBounds (i, getWidth()).toFloat();
        auto selected = i == current;

        g.setColour (theme::control);
        g.fillRoundedRectangle (b, 3.0f);

        auto led = juce::Rectangle<float> (b.getX() + 6.0f, b.getCentreY() - 3.5f, 7.0f, 7.0f);
        g.setColour (selected ? theme::ledOn : theme::ledOff);
        g.fillEllipse (led);

        if (selected)
        {
            g.setColour (theme::ledOn.withAlpha (0.25f));
            g.fillEllipse (led.expanded (3.0f));
        }

        g.setColour (selected ? theme::text : theme::textDim);
        g.setFont (theme::label (13.0f, true));
        g.drawFittedText (names[i], b.withTrimmedLeft (16.0f).withTrimmedRight (3.0f).toNearestInt(),
                          juce::Justification::centred, 1, 0.6f);
    }
}

void ChoiceButtons::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < names.size(); ++i)
        if (getButtonBounds (i, getWidth()).contains (e.getPosition()))
            attachment.setValueAsCompleteGesture ((float) i);
}

//==============================================================================
EnvelopeGraph::EnvelopeGraph (const SynthController& c, std::array<int, 4> adsr)
    : controller (c), indices (adsr)
{
    startTimerHz (20);
}

void EnvelopeGraph::timerCallback()
{
    std::array<int, 4> now;

    for (size_t i = 0; i < 4; ++i)
        now[i] = controller.getValue (indices[i]);

    if (now != shown)
    {
        shown = now;
        repaint();
    }
}

void EnvelopeGraph::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::lcdBackground);
    g.fillRoundedRectangle (r, 4.0f);

    auto area = r.reduced (6.0f, 6.0f);
    auto unit = area.getWidth() / (3.0f * 127.0f + 40.0f);   // A, D, R share the width, sustain plateau fixed
    auto a = (float) shown[0] * unit, d = (float) shown[1] * unit, rel = (float) shown[3] * unit;
    auto sustainY = area.getBottom() - (float) shown[2] / 127.0f * area.getHeight();
    auto x0 = area.getX(), bottom = area.getBottom();

    juce::Path p;
    p.startNewSubPath (x0, bottom);
    p.lineTo (x0 + a, area.getY());
    p.lineTo (x0 + a + d, sustainY);
    p.lineTo (x0 + a + d + 40.0f * unit, sustainY);
    p.lineTo (x0 + a + d + 40.0f * unit + rel, bottom);

    g.setColour (theme::lcdText);
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
SectionPanel::SectionPanel (const SectionDef& d, SynthController& controller, juce::AudioProcessorValueTreeState& state)
    : def (d)
{
    auto& device = controller.getDevice();

    for (auto& f : def.faders)
    {
        auto index = device.indexOf (f.id);
        jassert (index >= 0);
        faders.push_back (std::make_unique<FaderStrip> (device.getParams()[(size_t) index], state, f.label));
        addAndMakeVisible (*faders.back());
    }

    for (auto& s : def.selectors)
    {
        auto index = device.indexOf (s.id);
        jassert (index >= 0);
        selectors.push_back (std::make_unique<ChoiceButtons> (device.getParams()[(size_t) index], state, s.label, s.shortNames));
        addAndMakeVisible (*selectors.back());
    }

    if (def.envelope[0].isNotEmpty())
    {
        std::array<int, 4> idx;

        for (size_t i = 0; i < 4; ++i)
            idx[i] = device.indexOf (def.envelope[i]);

        graph = std::make_unique<EnvelopeGraph> (controller, idx);
        addAndMakeVisible (*graph);
    }
}

void SectionPanel::paint (juce::Graphics& g)
{
    paintSectionFrame (g, getLocalBounds(), def.title, def.accent);
}

int SectionPanel::getPreferredWidth() const
{
    auto w = 24 + (int) faders.size() * (FaderStrip::preferredWidth + FaderStrip::spacing);

    if (! selectors.empty() || graph != nullptr)
        w += (faders.empty() ? 0 : 8) + def.selectorWidth;

    return w;
}

void SectionPanel::resized()
{
    auto r = getLocalBounds().withTrimmedTop (titleHeight).reduced (12, 0).withTrimmedBottom (10);

    auto faderArea = r.removeFromLeft ((int) faders.size() * (FaderStrip::preferredWidth + FaderStrip::spacing));

    for (auto& f : faders)
        f->setBounds (faderArea.removeFromLeft (FaderStrip::preferredWidth + FaderStrip::spacing).withTrimmedRight (FaderStrip::spacing));

    if (! faders.empty())
        r.removeFromLeft (8);

    if (graph != nullptr)
    {
        graph->setBounds (r.removeFromTop (64));
        r.removeFromTop (6);
    }

    for (auto& s : selectors)
    {
        s->setBounds (r.removeFromTop (s->getPreferredHeight (r.getWidth())));
        r.removeFromTop (6);
    }
}

} // namespace sso
