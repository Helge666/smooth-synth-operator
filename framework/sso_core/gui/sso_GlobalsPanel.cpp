namespace sso
{

GlobalsPanel::GlobalsPanel (SynthController& c) : controller (c)
{
    auto& specs = controller.getDevice().getGlobalParams();

    for (size_t i = 0; i < specs.size(); ++i)
    {
        auto& spec = specs[i];
        Row row;
        row.index = (int) i;
        auto index = (int) i;

        if (spec.kind == ParamSpec::Kind::choice)
        {
            row.box = std::make_unique<juce::ComboBox>();
            row.box->addItemList (spec.choices, 1);
            row.box->onChange = [this, index, &spec, box = row.box.get()]
            {
                if (! refreshing)
                    controller.setGlobalValue (index, spec.minValue + box->getSelectedItemIndex());
            };
            addAndMakeVisible (*row.box);
        }
        else if (spec.kind == ParamSpec::Kind::toggle)
        {
            row.toggle = std::make_unique<juce::ToggleButton> ("On");
            row.toggle->setColour (juce::ToggleButton::textColourId, theme::text);
            row.toggle->setColour (juce::ToggleButton::tickColourId, theme::accent);
            row.toggle->onClick = [this, index, t = row.toggle.get()]
            {
                if (! refreshing)
                    controller.setGlobalValue (index, t->getToggleState() ? 1 : 0);
            };
            addAndMakeVisible (*row.toggle);
        }
        else
        {
            row.slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            row.slider->setRange (spec.minValue, spec.maxValue, 1.0);
            row.slider->setTextBoxStyle (juce::Slider::TextBoxRight, true, 70, 22);
            row.slider->textFromValueFunction = [&spec] (double v) { return spec.toText (juce::roundToInt (v)); };
            row.slider->getProperties().set ("bipolar", spec.kind == ParamSpec::Kind::bipolar);
            row.slider->setDoubleClickReturnValue (true, spec.defaultValue);
            row.slider->onValueChange = [this, index, s = row.slider.get()]
            {
                if (! refreshing)
                    controller.setGlobalValue (index, juce::roundToInt (s->getValue()));
            };
            addAndMakeVisible (*row.slider);
        }

        rows.push_back (std::move (row));
    }

    sendOnConnect.setColour (juce::ToggleButton::textColourId, theme::text);
    sendOnConnect.setColour (juce::ToggleButton::tickColourId, theme::accent);
    sendOnConnect.onClick = [this] { controller.setSendGlobalsOnConnect (sendOnConnect.getToggleState()); };
    addAndMakeVisible (sendOnConnect);

    readButton.setTooltip ("Replace these settings with the ones the synth has now");
    readButton.onClick = [this] { controller.requestGlobalsFromSynth(); };
    sendButton.setTooltip ("Set these settings in the synth now");
    sendButton.onClick = [this] { controller.sendGlobals(); };
    addAndMakeVisible (readButton);
    addAndMakeVisible (sendButton);

    controller.addChangeListener (this);
    refresh();

    juce::StringArray sections;
    for (auto& spec : specs)
        sections.addIfNotAlreadyThere (spec.section);

    setSize (width, 90 + sections.size() * sectionHeight + (int) rows.size() * rowHeight + 92);
}

GlobalsPanel::~GlobalsPanel()
{
    controller.removeChangeListener (this);
}

void GlobalsPanel::refresh()
{
    const juce::ScopedValueSetter<bool> svs (refreshing, true);
    auto& specs = controller.getDevice().getGlobalParams();

    for (auto& row : rows)
    {
        auto value = controller.getGlobalValue (row.index);
        auto& spec = specs[(size_t) row.index];

        if (row.box != nullptr)      row.box->setSelectedItemIndex (value - spec.minValue, juce::dontSendNotification);
        if (row.toggle != nullptr)   row.toggle->setToggleState (value != 0, juce::dontSendNotification);
        if (row.slider != nullptr)   row.slider->setValue (value, juce::dontSendNotification);
    }

    sendOnConnect.setToggleState (controller.getSendGlobalsOnConnect(), juce::dontSendNotification);
}

void GlobalsPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (14, 12);
    g.setColour (theme::text);
    g.setFont (theme::label (17.0f, true));
    g.drawText ("SYNTH GLOBALS", r.removeFromTop (24), juce::Justification::centredLeft);

    g.setColour (theme::textDim);
    g.setFont (theme::label (13.0f));
    g.drawFittedText ("The synth keeps these outside its sounds and stores them only from its own panel. "
                      "SSO can set them whenever it connects, like an external editor.",
                      r.removeFromTop (48), juce::Justification::topLeft, 3);
    r.removeFromTop (6);

    auto& specs = controller.getDevice().getGlobalParams();
    juce::String section;

    for (auto& row : rows)
    {
        auto& spec = specs[(size_t) row.index];

        if (spec.section != section)
        {
            section = spec.section;
            g.setColour (theme::accent);
            g.setFont (theme::label (13.0f, true).withExtraKerningFactor (0.1f));
            g.drawText (section.toUpperCase(), r.removeFromTop (sectionHeight), juce::Justification::bottomLeft);
        }

        g.setColour (theme::text);
        g.setFont (theme::label (14.0f));
        g.drawText (spec.name, r.removeFromTop (rowHeight).removeFromLeft (170), juce::Justification::centredLeft);
    }
}

void GlobalsPanel::resized()
{
    auto r = getLocalBounds().reduced (14, 12);
    r.removeFromTop (24 + 48 + 6);

    auto& specs = controller.getDevice().getGlobalParams();
    juce::String section;

    for (auto& row : rows)
    {
        auto& spec = specs[(size_t) row.index];

        if (spec.section != section)
        {
            section = spec.section;
            r.removeFromTop (sectionHeight);
        }

        auto line = r.removeFromTop (rowHeight).withTrimmedLeft (170).reduced (0, 3);

        if (row.box != nullptr)      row.box->setBounds (line.removeFromLeft (180));
        if (row.toggle != nullptr)   row.toggle->setBounds (line.removeFromLeft (100));
        if (row.slider != nullptr)   row.slider->setBounds (line);
    }

    r.removeFromTop (8);
    sendOnConnect.setBounds (r.removeFromTop (30));
    r.removeFromTop (8);
    auto buttons = r.removeFromTop (34);
    readButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).withTrimmedRight (4));
    sendButton.setBounds (buttons.withTrimmedLeft (4));
}

} // namespace sso
