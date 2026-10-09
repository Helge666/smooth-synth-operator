#pragma once

namespace sso
{

/** Connects the plugin's parameters (and therefore GUI and DAW automation) with the
    hardware synth.

    - Parameter changes from GUI/host are sent to the synth.
    - Patches received from the synth update the parameters without echoing back.
    - Keeps the raw patch so bytes without a parameter survive a round trip.

    Lives in the AudioProcessor; all public functions except the parameter callback
    are meant to be called on the message thread.
*/
class SynthController : private juce::AudioProcessorValueTreeState::Listener,
                        public juce::ChangeBroadcaster
{
public:
    SynthController (const DeviceDefinition&, juce::AudioProcessorValueTreeState&);
    ~SynthController() override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (const DeviceDefinition&);

    const DeviceDefinition& getDevice() const noexcept   { return device; }
    MidiConnection& getConnection() noexcept             { return connection; }

    //==============================================================================
    Patch getCurrentPatch() const;
    juce::String getPatchName() const;
    void setPatchName (const juce::String&);

    /** Makes `patch` the current sound and optionally sends it to the synth's edit buffer. */
    void loadPatch (const Patch&, bool sendToSynth);

    void requestFromSynth();
    void sendToSynth();
    void writeToSlot (int slot);

    /** Asks the synth for a stored sound. The answer arrives via onSlotReceived; it does
        not change the current sound in the editor (but the synth's edit buffer). */
    void requestSlot (int slot);
    std::function<void (const DeviceDefinition::IncomingPatch&)> onSlotReceived;   // message thread

    void setFrozen (bool);
    bool isFrozen() const noexcept;

    //==============================================================================
    DeviceDefinition::MidiSettings getMidiSettings() const noexcept;
    void setMidiChannel (int channel1to16);
    void setDeviceId (int id0to15);

    /** Opens the ports and remembers the choice for all instances. */
    void setMidiOutput (const juce::String& identifier);
    void setMidiInput (const juce::String& identifier);

    enum class SyncState { unknown, waitingForSynth, inSync, edited };
    SyncState getSyncState() const noexcept { return syncState; }
    juce::Time getLastReceiveTime() const noexcept { return lastReceive; }

    //==============================================================================
    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree&);

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleIncomingMessage (const juce::MidiMessage&);
    void applyPatchToParameters (const Patch&);
    void setSyncState (SyncState);
    juce::String settingsKey (const char* name) const   { return device.getDeviceId() + "." + name; }

    juce::SharedResourcePointer<AppSettings> settings;
    const DeviceDefinition& device;
    juce::AudioProcessorValueTreeState& state;
    MidiConnection connection;
    ParameterSender sender;

    juce::HashMap<juce::String, int> indexById;
    std::vector<juce::RangedAudioParameter*> parameters;

    Patch basePatch;        // raw data incl. bytes not covered by parameters
    std::atomic<int> applyingFromSynth { 0 };
    std::atomic<int> midiChannel { 1 }, deviceId { 0 };
    std::atomic<SyncState> syncState { SyncState::unknown };
    juce::Time lastReceive;

    JUCE_DECLARE_WEAK_REFERENCEABLE (SynthController)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthController)
};

} // namespace sso
