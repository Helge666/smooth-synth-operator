#pragma once

namespace sso
{

/** Makes sure only one SSO instance at a time controls a given synth
    (device type + MIDI output + channel) - across plugin instances in one host and
    across processes (standalone app, other hosts, sandboxed plugins).

    - Within a process the registry decides directly.
    - Across processes each process holds a named system-wide lock per synth while it
      owns it. A take-over request is a small file the owning process polls for; it then
      releases the synth and the requester acquires it.

    Use via juce::SharedResourcePointer<OwnershipRegistry>. Message thread only.
*/
class OwnershipRegistry : private juce::Timer
{
public:
    struct Member
    {
        virtual ~Member() = default;

        /** Identifies the synth this member wants to control; empty = no synth (never blocked). */
        virtual juce::String getOwnershipKey() const = 0;

        /** Shown to other instances, e.g. "Cubase - track 'Pad'". */
        virtual juce::String getInstanceDescription() const = 0;

        /** isOwner: whether this member now controls the synth.
            takenOver: true if the user explicitly took over (send the sound now). */
        virtual void ownershipChanged (bool isOwner, bool takenOver) = 0;
    };

    OwnershipRegistry();
    ~OwnershipRegistry() override;

    /** Registers a member. It only claims the synth when claim() is called (when the
        instance is really used), so instances a host creates just for scanning or probing
        never block the synth. */
    void add (Member&);
    void remove (Member&);

    /** Becomes owner if the synth is free. Idempotent. */
    void claim (Member&);
    void keyChanged (Member&);
    void descriptionChanged (Member&);

    bool isOwner (const Member&) const;

    /** Who controls the member's synth if it is not this member; empty = nobody. */
    juce::String getOwnerDescription (const Member&) const;

    /** Makes `member` the owner; may take a moment if another process owns the synth. */
    void takeOver (Member&);
    bool isTakingOver (const Member&) const;

    static juce::String makeKey (const juce::String& deviceId, const juce::String& portId, int channel);

    /** Problems worth knowing about later (MIDI loop, failed take-over); always written to
        %TEMP%/SSO-ownership/ownership.log. */
    void warn (const juce::String&) const;

    /** Step-by-step details for debugging host behaviour; only written while the file
        %TEMP%/SSO-ownership/verbose exists (checked at startup). */
    void log (const juce::String&) const;

private:
    struct Slot
    {
        std::unique_ptr<juce::InterProcessLock> lock;   // held while this process owns the synth
        Member* owner = nullptr;
        Member* requester = nullptr;                    // waiting for another process to release
        double requestStarted = 0;
    };

    void timerCallback() override;
    bool tryAcquire (const juce::String& key, Slot&);
    void release (const juce::String& key, Slot&);
    void writeOwnerFile (const juce::String& key, const Slot&) const;
    juce::File fileFor (const juce::String& key, const char* suffix) const;
    void notifyAll (const juce::String& key);
    juce::String keyOf (const Member&) const;
    juce::String describe (const Member&) const;
    void write (const juce::String&) const;

    std::map<juce::String, Slot> slots;
    std::vector<std::pair<Member*, juce::String>> members;
    std::set<Member*> claimed;          // members that are in use (want the synth when free)
    const juce::String processToken = juce::Uuid().toString();
    const bool verbose;

    JUCE_DECLARE_NON_COPYABLE (OwnershipRegistry)
};

} // namespace sso
