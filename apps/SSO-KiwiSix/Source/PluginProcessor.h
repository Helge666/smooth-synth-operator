#pragma once

#include <sso_kiwisix/sso_kiwisix.h>

class KiwiSixProcessor final : public sso::ProgrammerProcessor
{
public:
    KiwiSixProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KiwiSixProcessor)
};
