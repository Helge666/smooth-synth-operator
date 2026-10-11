#include "PluginProcessor.h"
#include "PluginEditor.h"

Kiwi3PProcessor::Kiwi3PProcessor()
    : ProgrammerProcessor (std::make_unique<sso::kiwi3p::Kiwi3PDevice>(), "SSO_Kiwi3P", "SSO Kiwi-3P")
{
    generator = sso::kiwi3p::createGenerator (static_cast<const sso::kiwi3p::Kiwi3PDevice&> (device));
}

juce::AudioProcessorEditor* Kiwi3PProcessor::createEditor()
{
    return new Kiwi3PEditor (*this);
}
