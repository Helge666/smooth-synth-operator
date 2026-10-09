#pragma once

#include <sso_kiwi3p/sso_kiwi3p.h>

class Kiwi3PProcessor final : public juce::AudioProcessor,
                              private juce::ChangeListener
{
public:
    Kiwi3PProcessor();
    ~Kiwi3PProcessor() override;

    void prepareToPlay (double sampleRate, int blockSize) override;
    void releaseResources() override                { forwarder.releaseHeldNotes(); }
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void updateTrackProperties (const TrackProperties&) override;

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    const sso::kiwi3p::Kiwi3PDevice device;
    juce::AudioProcessorValueTreeState parameters;
    sso::SynthController controller;
    sso::PresetLibrary library { device };
    std::unique_ptr<sso::PatchGenerator> generator = sso::kiwi3p::createGenerator (device);
    sso::NoteForwarder forwarder { controller };    // plays the track's notes on the synth

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    std::atomic<bool> forwardNotes { true };
    std::atomic<double> noteOffsetMs { 0.0 };
    std::shared_ptr<bool> aliveFlag = std::make_shared<bool> (true);   // guards async callbacks

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Kiwi3PProcessor)
};
