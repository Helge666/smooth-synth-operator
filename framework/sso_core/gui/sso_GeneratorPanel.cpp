namespace sso
{

GeneratorPanel::GeneratorPanel (PatchGenerator& g, SynthController& c)
    : generator (g), controller (c)
{
    for (auto& category : generator.getCategories())
    {
        auto* b = categoryButtons.add (new juce::TextButton (category.toUpperCase()));
        b->setTooltip ("Generate a new " + category + " sound");
        b->onClick = [this, category] { generate (category); };
        addAndMakeVisible (b);
    }

    for (auto& group : generator.getGroups())
    {
        auto* b = keepButtons.add (new juce::TextButton (group.substring (0, 3)));
        b->setClickingTogglesState (true);
        b->setColour (juce::TextButton::buttonOnColourId, theme::cream);
        b->setTooltip ("Keep " + group + " settings when generating");
        b->getProperties().set ("group", group);
        addAndMakeVisible (b);
    }

    initButton.setTooltip ("Start from a basic sound");
    initButton.onClick = [this]
    {
        auto keep = keptGroups();
        auto init = controller.getDevice().createInitPatch();

        // INIT honours KEEP as well: copy kept groups from the current sound
        if (! keep.isEmpty())
        {
            auto current = controller.getCurrentPatch();

            for (int i = 0; i < (int) controller.getDevice().getParams().size(); ++i)
                if (keep.contains (generator.getGroupOf (i)))
                    controller.getDevice().setValue (init, i, controller.getDevice().getValue (current, i));
        }

        apply (init, {});
    };

    variationButton.setTooltip ("Change the current sound slightly (amount: slider)");
    variationButton.onClick = [this] { varyCurrent(); };
    backButton.setTooltip ("Back to the sound before the last generate/variation");
    backButton.onClick = [this] { goBack(); };
    backButton.setEnabled (false);

    amount.setSliderStyle (juce::Slider::LinearHorizontal);
    amount.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    amount.setRange (0.02, 0.6);
    amount.setValue (0.15, juce::dontSendNotification);
    amount.setTooltip ("Variation amount");

    for (auto* comp : std::initializer_list<juce::Component*> { &initButton, &variationButton, &backButton, &amount })
        addAndMakeVisible (comp);
}

juce::StringArray GeneratorPanel::keptGroups() const
{
    juce::StringArray groups;

    for (auto* b : keepButtons)
        if (b->getToggleState())
            groups.add (b->getProperties()["group"].toString());

    return groups;
}

void GeneratorPanel::apply (const Patch& patch, const juce::String& category)
{
    history.push_back ({ controller.getCurrentPatch(), controller.getPresetRef() });

    if (history.size() > maxHistory)
        history.erase (history.begin());

    backButton.setEnabled (true);
    controller.silence();
    controller.loadPatch (patch, true);
    controller.setPresetRef ({ {}, generatedBank, category });
}

void GeneratorPanel::generate (const juce::String& category)
{
    apply (generator.generate (category, controller.getCurrentPatch(), keptGroups(), random), category);
}

void GeneratorPanel::varyCurrent()
{
    auto ref = controller.getPresetRef();
    auto patch = generator.vary (controller.getCurrentPatch(), (float) amount.getValue(), keptGroups(), random);
    apply (patch, ref.category);
}

void GeneratorPanel::goBack()
{
    if (history.empty())
        return;

    auto entry = history.back();
    history.pop_back();
    controller.silence();
    controller.loadPatch (entry.patch, true);
    controller.setPresetRef (entry.ref);
    backButton.setEnabled (! history.empty());
}

void GeneratorPanel::paint (juce::Graphics& g)
{
    paintSectionFrame (g, getLocalBounds(), "Generator", theme::yellow);

    g.setColour (theme::textDim);
    g.setFont (theme::label (12.0f));
    g.drawText ("KEEP", keepButtons.isEmpty() ? juce::Rectangle<int>() : keepButtons[0]->getBounds().withX (12).withWidth (40),
                juce::Justification::centredLeft);
}

void GeneratorPanel::resized()
{
    auto r = getLocalBounds().withTrimmedTop (SectionPanel::titleHeight).reduced (12, 0).withTrimmedBottom (10);

    auto columns = 4;
    auto w = (float) (r.getWidth() + 4) / (float) columns;

    for (int i = 0; i < categoryButtons.size(); ++i)
        categoryButtons[i]->setBounds (juce::Rectangle<float> ((float) r.getX() + (float) (i % columns) * w, (float) r.getY() + (float) (i / columns) * 34.0f,
                                                               w - 4.0f, 30.0f).toNearestInt());

    r.removeFromTop (((categoryButtons.size() + columns - 1) / columns) * 34 + 4);

    auto keepRow = r.removeFromTop (26);
    keepRow.removeFromLeft (40);
    auto kw = (float) (keepRow.getWidth() + 3) / (float) juce::jmax (1, keepButtons.size());

    for (int i = 0; i < keepButtons.size(); ++i)
        keepButtons[i]->setBounds (juce::Rectangle<float> ((float) keepRow.getX() + (float) i * kw, (float) keepRow.getY(), kw - 3.0f, 26.0f).toNearestInt());

    r.removeFromTop (8);
    auto bottom = r.removeFromTop (30);
    initButton.setBounds (bottom.removeFromLeft (64));
    bottom.removeFromLeft (4);
    backButton.setBounds (bottom.removeFromRight (64));
    bottom.removeFromRight (4);
    variationButton.setBounds (bottom.removeFromLeft (100));
    bottom.removeFromLeft (8);
    amount.setBounds (bottom);
}

} // namespace sso
