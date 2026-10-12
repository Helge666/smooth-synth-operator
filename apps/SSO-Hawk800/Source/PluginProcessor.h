#pragma once

#include <sso_hawk800/sso_hawk800.h>

class Hawk800Processor final : public sso::ProgrammerProcessor
{
public:
    Hawk800Processor();

    juce::AudioProcessorEditor* createEditor() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Hawk800Processor)
};
