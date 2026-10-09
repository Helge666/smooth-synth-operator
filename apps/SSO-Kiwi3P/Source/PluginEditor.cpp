#include "PluginEditor.h"

Kiwi3PEditor::Content::Content (Kiwi3PProcessor& p)
    : browser (p.library, p.controller),
      header (p.controller, "Kiwi-3P Programmer"),
      panel (p.controller, p.parameters, *p.generator)
{
    addAndMakeVisible (browser);
    addAndMakeVisible (header);
    addAndMakeVisible (panel);

    header.getDisplay().onRenameRequested = [this] { browser.renameCurrent(); };
}

void Kiwi3PEditor::Content::paint (juce::Graphics& g)
{
    g.fillAll (sso::theme::background);
}

void Kiwi3PEditor::Content::resized()
{
    auto r = getLocalBounds();
    browser.setVisible (browserVisible);

    if (browserVisible)
        browser.setBounds (r.removeFromLeft (browserWidth));

    r.reduce (20, 18);
    header.setBounds (r.removeFromTop (84));
    r.removeFromTop (14);
    panel.setBounds (r);
}

//==============================================================================
Kiwi3PEditor::Kiwi3PEditor (Kiwi3PProcessor& p)
    : AudioProcessorEditor (p), content (p)
{
    setLookAndFeel (&lookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);   // dialogs and menus

    auto& props = settings->get();
    content.browserVisible = props.getBoolValue ("kiwi3p.browserVisible", true);
    content.header.setBrowserVisible (content.browserVisible);
    content.header.onBrowserToggled = [this] (bool visible) { setBrowserVisible (visible); };
    addAndMakeVisible (content);

    // Default scale: fit 90% of the screen, never above 100%
    auto scale = props.getDoubleValue ("kiwi3p.scale", 0.0);

    if (scale <= 0.0)
    {
        auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userBounds;
        scale = juce::jmin (1.0, 0.9 * area.getWidth() / designWidth(), 0.9 * area.getHeight() / designHeight);
    }

    setResizable (true, true);
    updateAspect();
    setSize (juce::roundToInt (designWidth() * scale), juce::roundToInt (designHeight * scale));
}

Kiwi3PEditor::~Kiwi3PEditor()
{
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void Kiwi3PEditor::updateAspect()
{
    setResizeLimits (designWidth() / 3, designHeight / 3, designWidth() * 2, designHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) designWidth() / designHeight);
}

void Kiwi3PEditor::setBrowserVisible (bool visible)
{
    auto scale = (double) getHeight() / designHeight;
    content.browserVisible = visible;
    settings->get().setValue ("kiwi3p.browserVisible", visible);
    updateAspect();
    setSize (juce::roundToInt (designWidth() * scale), getHeight());
    resized();
}

void Kiwi3PEditor::paint (juce::Graphics& g)
{
    g.fillAll (sso::theme::background);
}

void Kiwi3PEditor::resized()
{
    auto scale = (float) getHeight() / (float) designHeight;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, designWidth(), designHeight);
    settings->get().setValue ("kiwi3p.scale", (double) scale);
}
