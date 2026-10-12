#include "PluginProcessor.h"
#include "PluginEditor.h"

Hawk800Processor::Hawk800Processor()
    : ProgrammerProcessor (std::make_unique<sso::hawk800::Hawk800Device>(), "SSO_Hawk800", "SSO Hawk-800")
{
    sso::theme::usePoly800Palette();    // before any window or look-and-feel is created
    generator = sso::hawk800::createGenerator (static_cast<const sso::hawk800::Hawk800Device&> (device));
}

juce::AudioProcessorEditor* Hawk800Processor::createEditor()
{
    return new Hawk800Editor (*this);
}
