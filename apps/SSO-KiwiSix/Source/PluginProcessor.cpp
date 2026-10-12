#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SSO_KiwiSix_Version.h"

KiwiSixProcessor::KiwiSixProcessor()
    : ProgrammerProcessor (std::make_unique<sso::kiwisix::KiwiSixDevice>(), "SSO_KiwiSix", "SSO KiwiSix")
{
    sso::theme::usePolySixPalette();    // before any window or look-and-feel is created
    generator = sso::kiwisix::createGenerator (static_cast<const sso::kiwisix::KiwiSixDevice&> (device));
    appVersion = SSO_APP_VERSION;
}

juce::AudioProcessorEditor* KiwiSixProcessor::createEditor()
{
    return new KiwiSixEditor (*this);
}
