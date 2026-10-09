#include "PluginProcessor.h"
#include "PluginEditor.h"

Kiwi3PProcessor::Kiwi3PProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "SSO_Kiwi3P", sso::SynthController::createParameterLayout (device)),
      controller (device, parameters)
{
}

Kiwi3PProcessor::~Kiwi3PProcessor() = default;

void Kiwi3PProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    buffer.clear();
}

void Kiwi3PProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.removeChild (state.getChildWithName ("SynthController"), nullptr);
    state.appendChild (controller.toValueTree(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void Kiwi3PProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    auto controllerState = state.getChildWithName ("SynthController");
    state.removeChild (controllerState, nullptr);

    if (controllerState.isValid())
        controller.fromValueTree (controllerState);     // restores raw patch + parameters
    else
        parameters.replaceState (state);
}

juce::AudioProcessorEditor* Kiwi3PProcessor::createEditor()
{
    return new Kiwi3PEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Kiwi3PProcessor();
}
