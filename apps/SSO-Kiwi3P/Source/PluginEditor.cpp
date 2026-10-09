#include "PluginEditor.h"

Kiwi3PEditor::Kiwi3PEditor (Kiwi3PProcessor& p)
    : AudioProcessorEditor (p),
      connectionBar (p.controller),
      panel (p.device, p.parameters)
{
    addAndMakeVisible (connectionBar);
    viewport.setViewedComponent (&panel, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    setResizable (true, true);
    setResizeLimits (900, 400, 2400, 1600);
    setSize (1400, 860);
}

void Kiwi3PEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2a2a2e));
}

void Kiwi3PEditor::resized()
{
    auto r = getLocalBounds();
    connectionBar.setBounds (r.removeFromTop (48));
    viewport.setBounds (r.reduced (4));

    auto width = viewport.getMaximumVisibleWidth();
    panel.setSize (width, panel.getRequiredHeight (width));
}
