namespace sso
{

void paintSectionFrame (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title, juce::Colour accent,
                        const juce::String& subtitle)
{
    auto r = bounds.toFloat();

    if (theme::tabbedSections)
    {
        // PolySix style: panel field with a thin outline, title on a cream tab in the corner
        g.setColour (theme::panel);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (theme::sectionOutline);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

        auto font = theme::label (15.0f, true).withExtraKerningFactor (0.18f);
        auto text = title.toUpperCase();
        auto tabWidth = juce::GlyphArrangement::getStringWidthInt (font, text) + 30;
        auto tab = juce::Rectangle<float> (r.getX(), r.getY(), (float) tabWidth, (float) SectionPanel::titleHeight - 4.0f);

        juce::Path p;
        p.addRoundedRectangle (tab.getX(), tab.getY(), tab.getWidth(), tab.getHeight(), 4.0f, 4.0f, true, false, false, true);
        g.setColour (theme::tabFill);
        g.fillPath (p);
        g.setColour (theme::tabText);
        g.setFont (font);
        g.drawText (text, tab, juce::Justification::centred);

        if (subtitle.isNotEmpty())
        {
            g.setColour (theme::textDim);
            g.setFont (theme::label (12.0f).withExtraKerningFactor (0.08f));
            g.drawText (subtitle.toUpperCase(), tab.withX (tab.getRight() + 10.0f).withRight (r.getRight() - 8.0f),
                        juce::Justification::centredLeft);
        }

        return;
    }

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
KnobStrip::KnobStrip (const ParamSpec& s, juce::AudioProcessorValueTreeState& state, const juce::String& l, int size)
    : spec (s), knobSize (size), label (l.isEmpty() ? s.name.toUpperCase() : l)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (220);
    slider.setDoubleClickReturnValue (true, spec.defaultValue);
    slider.getProperties().set ("bipolar", spec.kind == ParamSpec::Kind::bipolar);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, spec.id, slider);

    setTooltip (spec.section + " " + spec.name + "  (drag up/down, double-click: default)");
    slider.setTooltip (getTooltip());
}

void KnobStrip::resized()
{
    slider.setBounds (getLocalBounds().removeFromTop (knobSize).withSizeKeepingCentre (knobSize, knobSize));
}

void KnobStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().withTrimmedTop (knobSize + 2).removeFromTop (34);
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
                              const juce::String& l, const juce::StringArray& shortNames, int columns,
                              std::vector<int> flagsOfValue)
    : spec (s),
      label (l.isEmpty() ? s.name.toUpperCase() : l),
      names (! shortNames.isEmpty() ? shortNames
                                    : (s.kind == ParamSpec::Kind::toggle ? juce::StringArray { "OFF", "ON" } : s.choices)),
      maxColumns (columns),
      flags (std::move (flagsOfValue)),
      attachment (*state.getParameter (s.id), [this] (float v) { current = juce::roundToInt (v); repaint(); })
{
    attachment.sendInitialUpdate();
    setTooltip (spec.section + " " + spec.name);
}

int ChoiceButtons::getColumns (int width) const
{
    // as many as fit with a minimum button width of 36 px
    auto fit = juce::jlimit (1, names.size(), (width + 3) / (36 + 3));
    return maxColumns > 0 ? juce::jmin (fit, maxColumns) : fit;
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
        auto selected = isLit (i);

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

bool ChoiceButtons::isLit (int button) const
{
    if (flags.empty())
        return button == current;

    return juce::isPositiveAndBelow (current, (int) flags.size()) && (flags[(size_t) current] & (1 << button)) != 0;
}

void ChoiceButtons::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < names.size(); ++i)
    {
        if (! getButtonBounds (i, getWidth()).contains (e.getPosition()))
            continue;

        if (flags.empty())
        {
            attachment.setValueAsCompleteGesture ((float) i);
            return;
        }

        // Switch this button, then pick the value with exactly that combination
        auto wanted = (juce::isPositiveAndBelow (current, (int) flags.size()) ? flags[(size_t) current] : 0) ^ (1 << i);

        for (size_t v = 0; v < flags.size(); ++v)
            if (flags[v] == wanted)
                attachment.setValueAsCompleteGesture ((float) v);

        return;
    }
}

//==============================================================================
ToggleGroup::ToggleGroup (const std::vector<const ParamSpec*>& specs, juce::AudioProcessorValueTreeState& state,
                          const juce::String& l, const juce::StringArray& n)
    : label (l), names (n)
{
    states.resize (specs.size(), 0);
    juce::StringArray tips;

    for (size_t i = 0; i < specs.size(); ++i)
    {
        attachments.push_back (std::make_unique<juce::ParameterAttachment> (*state.getParameter (specs[i]->id),
            [this, i] (float v) { states[i] = juce::roundToInt (v); repaint(); }));
        attachments.back()->sendInitialUpdate();
        tips.add (specs[i]->name);
    }

    if (! specs.empty())
        setTooltip (specs.front()->section + ": " + tips.joinIntoString (", ") + " (each on/off)");
}

int ToggleGroup::getPreferredHeight (int) const
{
    return choiceLabelHeight + choiceButtonHeight + 3;
}

juce::Rectangle<int> ToggleGroup::getButtonBounds (int index, int width) const
{
    auto n = juce::jmax (1, names.size());
    auto w = (width - (n - 1) * 3) / n;
    return { index * (w + 3), choiceLabelHeight, w, choiceButtonHeight };
}

void ToggleGroup::paint (juce::Graphics& g)
{
    g.setColour (theme::textDim);
    g.setFont (theme::label (12.0f));
    g.drawText (label, 0, 0, getWidth(), choiceLabelHeight - 2, juce::Justification::bottomLeft);

    for (int i = 0; i < names.size() && i < (int) states.size(); ++i)
    {
        auto b = getButtonBounds (i, getWidth()).toFloat();
        auto on = states[(size_t) i] != 0;

        g.setColour (theme::control);
        g.fillRoundedRectangle (b, 3.0f);

        auto led = juce::Rectangle<float> (b.getX() + 6.0f, b.getCentreY() - 3.5f, 7.0f, 7.0f);
        g.setColour (on ? theme::ledOn : theme::ledOff);
        g.fillEllipse (led);

        if (on)
        {
            g.setColour (theme::ledOn.withAlpha (0.25f));
            g.fillEllipse (led.expanded (3.0f));
        }

        g.setColour (on ? theme::text : theme::textDim);
        g.setFont (theme::label (13.0f, true));
        g.drawFittedText (names[i], b.withTrimmedLeft (16.0f).withTrimmedRight (3.0f).toNearestInt(),
                          juce::Justification::centred, 1, 0.6f);
    }
}

void ToggleGroup::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < names.size() && i < (int) attachments.size(); ++i)
        if (getButtonBounds (i, getWidth()).contains (e.getPosition()))
            attachments[(size_t) i]->setValueAsCompleteGesture (states[(size_t) i] != 0 ? 0.0f : 1.0f);
}

//==============================================================================
EnvelopeGraph::EnvelopeGraph (const SynthController& c, std::vector<int> paramIndices)
    : controller (c), indices (std::move (paramIndices))
{
    jassert (indices.size() == 4 || indices.size() == 6);

    for (auto index : indices)
        maxValues.push_back (juce::jmax (1, c.getDevice().getParams()[(size_t) index].maxValue));

    shown.assign (indices.size(), -1);
    startTimerHz (20);
}

void EnvelopeGraph::timerCallback()
{
    std::vector<int> now;

    for (auto index : indices)
        now.push_back (controller.getValue (index));

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
    auto norm = [this] (size_t i) { return (float) shown[i] / (float) maxValues[i]; };
    auto x0 = area.getX(), bottom = area.getBottom();
    auto level = [&] (float n) { return bottom - n * area.getHeight(); };
    juce::Path p;
    p.startNewSubPath (x0, bottom);

    if (indices.size() == 6)
    {
        // A, D (to the break point), slope (to sustain), a fixed plateau and R share the width
        auto unit = area.getWidth() / (4.0f + 40.0f / 127.0f);
        auto x = x0 + norm (0) * unit;
        p.lineTo (x, area.getY());
        x += norm (1) * unit;
        p.lineTo (x, level (norm (2)));
        x += norm (3) * unit;
        p.lineTo (x, level (norm (4)));
        x += 40.0f / 127.0f * unit;
        p.lineTo (x, level (norm (4)));
        p.lineTo (x + norm (5) * unit, bottom);
    }
    else
    {
        auto unit = area.getWidth() / (3.0f + 40.0f / 127.0f);   // A, D, R share the width, sustain plateau fixed
        auto a = norm (0) * unit, d = norm (1) * unit, rel = norm (3) * unit;
        auto sustainY = level (norm (2));
        p.lineTo (x0 + a, area.getY());
        p.lineTo (x0 + a + d, sustainY);
        auto plateau = 40.0f / 127.0f * unit;
        p.lineTo (x0 + a + d + plateau, sustainY);
        p.lineTo (x0 + a + d + plateau + rel, bottom);
    }

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
        auto& spec = device.getParams()[(size_t) index];

        if (def.knobs)
            faders.push_back (std::make_unique<KnobStrip> (spec, state, f.label, def.knobSize));
        else
            faders.push_back (std::make_unique<FaderStrip> (spec, state, f.label));

        addAndMakeVisible (*faders.back());
    }

    for (auto& s : def.selectors)
    {
        auto index = device.indexOf (s.id);
        jassert (index >= 0);
        selectors.push_back (std::make_unique<ChoiceButtons> (device.getParams()[(size_t) index], state, s.label, s.shortNames,
                                                              s.columns, s.flags));
        addAndMakeVisible (*selectors.back());
    }

    for (auto& t : def.toggles)
    {
        std::vector<const ParamSpec*> specs;

        for (auto& id : t.ids)
        {
            auto index = device.indexOf (id);
            jassert (index >= 0);
            specs.push_back (&device.getParams()[(size_t) index]);
        }

        toggles.push_back (std::make_unique<ToggleGroup> (specs, state, t.label, t.names));
        addAndMakeVisible (*toggles.back());
    }

    if (! def.envelope.empty())
    {
        std::vector<int> idx;

        for (auto& id : def.envelope)
        {
            idx.push_back (device.indexOf (id));
            jassert (idx.back() >= 0);
        }

        graph = std::make_unique<EnvelopeGraph> (controller, idx);
        addAndMakeVisible (*graph);
    }
}

void SectionPanel::paint (juce::Graphics& g)
{
    paintSectionFrame (g, getLocalBounds(), def.title, def.accent, def.subtitle);
}

int SectionPanel::stripWidth() const noexcept
{
    return def.knobs ? KnobStrip::widthFor (def.knobSize) + KnobStrip::spacing
                     : FaderStrip::preferredWidth + FaderStrip::spacing;
}

int SectionPanel::stripsPerRow() const noexcept
{
    return def.knobColumns > 0 ? juce::jmin (def.knobColumns, (int) faders.size()) : (int) faders.size();
}

int SectionPanel::getPreferredWidth() const
{
    auto w = 24 + stripsPerRow() * stripWidth();

    if (! selectors.empty() || ! toggles.empty() || graph != nullptr)
        w += (faders.empty() ? 0 : 8) + def.selectorWidth;

    return w;
}

void SectionPanel::resized()
{
    auto r = getLocalBounds().withTrimmedTop (titleHeight + (def.knobs ? 8 : 0)).reduced (12, 0).withTrimmedBottom (10);
    auto spacing = def.knobs ? KnobStrip::spacing : FaderStrip::spacing;

    auto faderArea = r.removeFromLeft (stripsPerRow() * stripWidth());
    auto knobHeight = def.knobSize + KnobStrip::textHeight;
    auto perRow = juce::jmax (1, stripsPerRow());

    for (size_t i = 0; i < faders.size(); ++i)
    {
        auto column = (int) i % perRow, row = (int) i / perRow;
        auto strip = juce::Rectangle<int> (faderArea.getX() + column * stripWidth(), faderArea.getY(),
                                           stripWidth(), faderArea.getHeight()).withTrimmedRight (spacing);

        faders[i]->setBounds (def.knobs ? strip.withY (strip.getY() + row * (knobHeight + 4)).withHeight (knobHeight) : strip);
    }

    if (! faders.empty())
        r.removeFromLeft (8);

    if (graph != nullptr)
    {
        graph->setBounds (r.removeFromTop (def.graphHeight));
        r.removeFromTop (6);
    }

    for (auto& s : selectors)
    {
        s->setBounds (r.removeFromTop (s->getPreferredHeight (r.getWidth())));
        r.removeFromTop (6);
    }

    for (auto& t : toggles)
    {
        t->setBounds (r.removeFromTop (t->getPreferredHeight (r.getWidth())));
        r.removeFromTop (6);
    }
}

} // namespace sso
