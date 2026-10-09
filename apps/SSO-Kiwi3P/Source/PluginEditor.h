#pragma once

#include "PluginProcessor.h"

class Kiwi3PEditor final : public juce::AudioProcessorEditor
{
public:
    explicit Kiwi3PEditor (Kiwi3PProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    sso::ConnectionBar connectionBar;
    sso::GenericParamPanel panel;
    juce::Viewport viewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Kiwi3PEditor)
};
