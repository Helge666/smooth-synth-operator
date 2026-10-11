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
                        private OwnershipRegistry::Member,
                        private juce::Timer,
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

    /** Current value of a parameter (any thread). */
    int getValue (int paramIndex) const noexcept   { return sender.getValue (paramIndex); }

    /** The parameter the user (GUI or host) changed last, and a counter that increments
        with every such change - for "last touched" displays. -1 = none yet. */
    int getLastUserChange() const noexcept          { return lastUserParam.load(); }
    uint32_t getUserChangeCount() const noexcept    { return userChangeCount.load(); }

    /** Library preset the current sound was loaded from (empty file = none). */
    struct PresetRef
    {
        juce::File file;
        juce::String bank, category;
    };

    const PresetRef& getPresetRef() const noexcept  { return presetRef; }
    void setPresetRef (PresetRef);

    /** Makes `patch` the current sound and optionally sends it to the synth's edit buffer. */
    void loadPatch (const Patch&, bool sendToSynth);

    void requestFromSynth();
    void sendToSynth();
    void writeToSlot (int slot);
    void writePatchToSlot (const Patch&, int slot);

    /** Sends a message (e.g. notes) to the synth, in order with all other output. Any thread. */
    void sendMidi (const juce::MidiMessage&);

    /** Stops all sound on the synth immediately (also release tails). Call before switching sounds. */
    void silence();

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

    /** Playing the host track's notes on the synth (see NoteForwarder); remembered for all instances. */
    bool getForwardNotes() const;
    void setForwardNotes (bool);
    double getNoteOffsetMs() const;
    void setNoteOffsetMs (double);

    //==============================================================================
    /** The synth's global settings (see DeviceDefinition::getGlobalParams), kept in the
        app settings. With "send on connect" they are sent whenever this instance starts
        controlling the synth - for synths that only store globals from their own panel. */
    int getGlobalValue (int globalIndex) const;
    void setGlobalValue (int globalIndex, int value);       // stores and sends it now
    bool getSendGlobalsOnConnect() const;
    void setSendGlobalsOnConnect (bool);
    void sendGlobals();
    void requestGlobalsFromSynth();                         // the answer replaces the stored values
    std::function<void()> onGlobalsReceived;                // message thread

    /** Opens the ports and remembers the choice for all instances. */
    void setMidiOutput (const juce::String& identifier);
    void setMidiInput (const juce::String& identifier);

    //==============================================================================
    /** Only one instance controls a synth at a time (see OwnershipRegistry). An inactive
        instance keeps its sound but sends nothing until the user takes over. */
    bool isActive() const noexcept                  { return active.load(); }
    juce::String getOwnerDescription() const        { return registry->getOwnerDescription (*this); }
    void takeOver()                                 { registry->takeOver (*this); }

    /** Call when the instance is really used (prepared for playback, editor opened, project
        state restored). Only then does it claim the synth - instances a host creates just
        to scan or probe the plugin never block it. */
    void startUsing()                               { registry->claim (*this); }
    bool isTakingOver() const                       { return registry->isTakingOver (*this); }

    /** How other instances see this one, e.g. "Cubase - track Pad". */
    void setInstanceDescription (const juce::String&);

    /** Whether the synth answers: heard from recently (any message, also clock), or asked
        and silent. Asked with a short read-only request on connect and after 10 s of silence. */
    enum class Presence { unknown, present, absent };
    Presence getPresence() const;

    enum class SyncState { unknown, waitingForSynth, inSync, edited };
    SyncState getSyncState() const noexcept { return syncState; }
    juce::Time getLastReceiveTime() const noexcept { return lastReceive; }

    //==============================================================================
    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree&);

private:
    // OwnershipRegistry::Member
    juce::String getOwnershipKey() const override;
    juce::String getInstanceDescription() const override   { return description; }
    void ownershipChanged (bool isOwner, bool takenOver) override;
    void connectionChanged();
    void sendPendingSoundIfPossible();
    void sendGlobalsIfDue();
    void timerCallback() override;
    void probeSynth();
    void storeGlobals();

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleIncomingMessage (const juce::MidiMessage&);
    void applyPatchToParameters (const Patch&);
    void setSyncState (SyncState);
    juce::String settingsKey (const char* name) const   { return device.getDeviceId() + "." + name; }

    juce::SharedResourcePointer<AppSettings> settings;
    juce::SharedResourcePointer<OwnershipRegistry> registry;
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
    std::atomic<int> lastUserParam { -1 };
    std::atomic<uint32_t> userChangeCount { 0 };
    PresetRef presetRef;
    std::atomic<bool> active { true };
    bool pendingSend = false;           // a restored sound still has to go to the synth
    std::vector<int> globalValues;
    juce::String globalsSentTo;         // output the globals were last sent to (empty = not sent)
    bool globalsRequested = false;      // the next global dump replaces the stored values
    double probeSentMs = 0;             // 0 = no probe outstanding
    bool probeUnanswered = false;       // the last probe timed out and nothing was heard since
    bool registered = false;
    juce::String registeredKey;
    juce::String description { "SSO" };

    JUCE_DECLARE_WEAK_REFERENCEABLE (SynthController)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthController)
};

} // namespace sso
