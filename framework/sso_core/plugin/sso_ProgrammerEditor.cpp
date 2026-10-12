namespace sso
{

static int cheeks() noexcept   { return theme::woodCheeks ? ProgrammerEditor::cheekWidth : 0; }

ProgrammerEditor::Content::Content (ProgrammerProcessor& p, std::unique_ptr<juce::Component> devicePanel, const Layout& l)
    : layout (l),
      browser (p.library, p.controller),
      header (p.controller, l.productLine, l.version, p.getName(), &p.aiControl),
      panel (std::move (devicePanel)),
      tuneOverlay (p.controller),
      inactiveOverlay (p.controller)
{
    addAndMakeVisible (browser);
    addAndMakeVisible (header);
    addAndMakeVisible (*panel);
    addChildComponent (tuneOverlay);        // shows itself while tuning
    addChildComponent (inactiveOverlay);    // shows itself while the instance is inactive

    header.getDisplay().onRenameRequested = [this] { browser.renameCurrent(); };
}

juce::Rectangle<int> ProgrammerEditor::Content::consoleArea() const
{
    auto r = getLocalBounds();

    if (browserVisible)
        r.removeFromLeft (browserWidth);

    return r.reduced (cheeks(), 0);
}

void ProgrammerEditor::Content::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
    auto console = consoleArea();

    if (theme::headerStrip)
    {
        g.setColour (theme::headerStripColour);
        g.fillRect (console.withHeight (18 + 84 + layout.panelGap / 2));
    }

    if (theme::woodCheeks)
    {
        auto paintCheek = [&g] (juce::Rectangle<int> r)
        {
            g.setColour (theme::wood);
            g.fillRect (r);
            g.setColour (juce::Colours::black.withAlpha (0.18f));

            for (auto x = r.getX(); x < r.getRight(); x += 7)
                g.fillRect (x, r.getY(), 2, r.getHeight());
        };

        paintCheek (console.withWidth (cheekWidth).translated (-cheekWidth, 0));
        paintCheek (console.withX (console.getRight()).withWidth (cheekWidth));
    }
}

void ProgrammerEditor::Content::resized()
{
    browser.setVisible (browserVisible);

    if (browserVisible)
        browser.setBounds (getLocalBounds().removeFromLeft (browserWidth));

    inactiveOverlay.setBounds (getLocalBounds());

    auto r = consoleArea().reduced (20, 18);
    header.setBounds (r.removeFromTop (84));
    tuneOverlay.setBounds (getLocalBounds());
    tuneOverlay.setUncovered (header.getBounds());
    r.removeFromTop (layout.panelGap);
    panel->setBounds (r);
}

//==============================================================================
ProgrammerEditor::ProgrammerEditor (ProgrammerProcessor& p, std::unique_ptr<juce::Component> panel, Layout layout)
    : AudioProcessorEditor (p), programmer (p), content (p, std::move (panel), layout)
{
    p.controller.startUsing();     // an open editor means this instance is in use
    setLookAndFeel (&lookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);   // dialogs and menus

    auto& props = settings->get();
    content.browserVisible = props.getBoolValue (settingsKey ("browserVisible"), true);
    content.header.setBrowserVisible (content.browserVisible);
    content.header.onBrowserToggled = [this] (bool visible) { setBrowserVisible (visible); };
    addAndMakeVisible (content);

    // Default scale: fit 90% of the screen, never above 100%
    auto scale = props.getDoubleValue (settingsKey ("scale"), 0.0);
    auto height = content.layout.designHeight;

    if (scale <= 0.0)
    {
        auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userBounds;
        scale = juce::jmin (1.0, 0.9 * area.getWidth() / designWidth(), 0.9 * area.getHeight() / height);
    }

    setResizable (true, true);
    updateAspect();
    setSize (juce::roundToInt (designWidth() * scale), juce::roundToInt (height * scale));

    // Space plays the test phrase - only in the standalone app; in a DAW it is the transport key.
    if (isStandalone())
    {
        setWantsKeyboardFocus (true);
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ProgrammerEditor> (this)]
        {
            if (safe != nullptr && safe->isShowing())
                safe->grabKeyboardFocus();
        });
    }
}

ProgrammerEditor::~ProgrammerEditor()
{
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

juce::String ProgrammerEditor::settingsKey (const char* name) const
{
    return programmer.device.getDeviceId() + "." + name;
}

int ProgrammerEditor::designWidth() const noexcept
{
    return content.layout.consoleWidth + 2 * cheeks() + (content.browserVisible ? browserWidth : 0);
}

bool ProgrammerEditor::isStandalone() const noexcept
{
    return processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
}

bool ProgrammerEditor::keyPressed (const juce::KeyPress& key)
{
    if (isStandalone() && key == juce::KeyPress::spaceKey)
    {
        content.header.toggleTestNotes();
        return true;
    }

    return false;
}

void ProgrammerEditor::updateAspect()
{
    auto height = content.layout.designHeight;
    setResizeLimits (designWidth() / 3, height / 3, designWidth() * 2, height * 2);
    getConstrainer()->setFixedAspectRatio ((double) designWidth() / height);
}

void ProgrammerEditor::setBrowserVisible (bool visible)
{
    auto scale = (double) getHeight() / content.layout.designHeight;
    content.browserVisible = visible;
    settings->get().setValue (settingsKey ("browserVisible"), visible);
    updateAspect();
    setSize (juce::roundToInt (designWidth() * scale), getHeight());
    resized();
}

void ProgrammerEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
}

void ProgrammerEditor::resized()
{
    auto scale = (float) getHeight() / (float) content.layout.designHeight;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, designWidth(), content.layout.designHeight);
    settings->get().setValue (settingsKey ("scale"), (double) scale);
}

} // namespace sso
