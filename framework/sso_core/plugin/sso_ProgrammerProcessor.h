#pragma once

namespace sso
{

/** The audio processor shared by all programmers: parameters, synth controller, library,
    generator and note forwarding. The track's notes go to the synth; the only sound it makes
    itself is the reference tone while tuning (TUNE).

    A programmer derives from it, passes its device definition, creates its generator and
    returns its editor.
*/
class ProgrammerProcessor : public juce::AudioProcessor,
                            private juce::ChangeListener
{
public:
    ProgrammerProcessor (std::unique_ptr<const DeviceDefinition>, const juce::String& stateType, const juce::String& name);
    ~ProgrammerProcessor() override;

    void prepareToPlay (double sampleRate, int blockSize) override;
    void releaseResources() override                { forwarder.releaseHeldNotes(); }
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return pluginName; }
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

private:
    const juce::String pluginName;
    const std::unique_ptr<const DeviceDefinition> ownedDevice;

public:
    const DeviceDefinition& device;
    juce::AudioProcessorValueTreeState parameters;
    SynthController controller;
    PresetLibrary library { device };
    std::unique_ptr<PatchGenerator> generator;      // created by the derived programmer
    NoteForwarder forwarder { controller };         // plays the track's notes on the synth
    TuneTone tuneTone;                              // the 440 Hz reference while tuning

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    std::atomic<bool> forwardNotes { true };
    std::atomic<double> noteOffsetMs { 0.0 };
    std::shared_ptr<bool> aliveFlag = std::make_shared<bool> (true);   // guards async callbacks

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgrammerProcessor)
};

} // namespace sso
