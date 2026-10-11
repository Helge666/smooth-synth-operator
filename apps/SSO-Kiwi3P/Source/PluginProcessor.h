#pragma once

#include <sso_kiwi3p/sso_kiwi3p.h>

class Kiwi3PProcessor final : public sso::ProgrammerProcessor
{
public:
    Kiwi3PProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Kiwi3PProcessor)
};
