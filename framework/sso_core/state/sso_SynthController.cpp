namespace sso
{

juce::AudioProcessorValueTreeState::ParameterLayout SynthController::createParameterLayout (const DeviceDefinition& device)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (auto& spec : device.getParams())
    {
        auto pid = juce::ParameterID { spec.id, 1 };
        auto label = spec.section + " " + spec.name;

        if (spec.kind == ParamSpec::Kind::choice)
        {
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, label, spec.choices,
                                                                      spec.defaultValue - spec.minValue));
        }
        else if (spec.kind == ParamSpec::Kind::toggle)
        {
            layout.add (std::make_unique<juce::AudioParameterBool> (pid, label, spec.defaultValue != 0,
                juce::AudioParameterBoolAttributes().withStringFromValueFunction (
                    [s = spec] (bool v, int) { return s.toText (v ? 1 : 0); })));
        }
        else
        {
            layout.add (std::make_unique<juce::AudioParameterInt> (pid, label, spec.minValue, spec.maxValue, spec.defaultValue,
                juce::AudioParameterIntAttributes().withStringFromValueFunction (
                    [s = spec] (int v, int) { return s.toText (v); })));
        }
    }

    return layout;
}

static int intValueOf (const juce::RangedAudioParameter& p, int minValue)
{
    // AudioParameterChoice reports its index, AudioParameterBool 0/1.
    auto v = juce::roundToInt (p.convertFrom0to1 (p.getValue()));
    return dynamic_cast<const juce::AudioParameterChoice*> (&p) != nullptr ? v + minValue : v;
}

SynthController::SynthController (const DeviceDefinition& d, juce::AudioProcessorValueTreeState& s)
    : device (d), state (s),
      sender (d, connection, [this] { return getMidiSettings(); })
{
    auto& specs = device.getParams();

    for (size_t i = 0; i < specs.size(); ++i)
    {
        indexById.set (specs[i].id, (int) i);
        auto* p = state.getParameter (specs[i].id);
        jassert (p != nullptr);
        parameters.push_back (p);
        state.addParameterListener (specs[i].id, this);
        sender.setValue ((int) i, intValueOf (*p, specs[i].minValue), false);
    }

    basePatch = device.createInitPatch();

    connection.onMessageReceived = [this] (const juce::MidiMessage& m)
    {
        if (! m.isSysEx())
            return;

        juce::MessageManager::callAsync ([weak = juce::WeakReference<SynthController> (this), m]
        {
            if (weak != nullptr)
                weak->handleIncomingMessage (m);
        });
    };

    connection.onStateChanged = [this] { connectionChanged(); };

    auto& props = settings->get();
    midiChannel = props.getIntValue (settingsKey ("channel"), 1);
    deviceId = props.getIntValue (settingsKey ("deviceId"), 0);
    connection.setOutput (props.getValue (settingsKey ("midiOut")));
    connection.setInput (props.getValue (settingsKey ("midiIn")));

    registeredKey = getOwnershipKey();
    registered = true;
    registry->add (*this);
}

SynthController::~SynthController()
{
    registry->remove (*this);

    connection.onMessageReceived = nullptr;
    connection.onStateChanged = nullptr;

    for (auto& spec : device.getParams())
        state.removeParameterListener (spec.id, this);
}

//==============================================================================
juce::String SynthController::getOwnershipKey() const
{
    return OwnershipRegistry::makeKey (device.getDeviceId(), connection.getOutputId(), midiChannel.load());
}

void SynthController::ownershipChanged (bool isOwner, bool takenOver)
{
    auto wasActive = active;
    active = isOwner;
    sender.setMuted (! active);

    if (active && ! wasActive && takenOver)
        pendingSend = true;     // the user wants this instance's sound on the synth

    sendPendingSoundIfPossible();
    sendChangeMessage();
}

void SynthController::connectionChanged()
{
    // Port changes change which synth this instance talks to
    if (registered && getOwnershipKey() != registeredKey)
    {
        registeredKey = getOwnershipKey();
        registry->keyChanged (*this);
    }

    sendPendingSoundIfPossible();
    sendChangeMessage();
}

void SynthController::sendPendingSoundIfPossible()
{
    if (! pendingSend || ! active || ! connection.isOutputOpen())
        return;

    pendingSend = false;
    silence();
    sendToSynth();
}

void SynthController::setInstanceDescription (const juce::String& d)
{
    if (d == description)
        return;

    description = d;
    registry->descriptionChanged (*this);
}

//==============================================================================
void SynthController::parameterChanged (const juce::String& parameterID, float newValue)
{
    auto index = indexById[parameterID];
    auto& spec = device.getParams()[(size_t) index];
    auto value = juce::roundToInt (newValue);

    if (spec.kind == ParamSpec::Kind::choice)
        value += spec.minValue;

    auto fromSynth = applyingFromSynth.load() > 0;
    sender.setValue (index, value, ! fromSynth);

    if (! fromSynth)
    {
        lastUserParam = index;
        ++userChangeCount;
    }

    if (! fromSynth && syncState != SyncState::edited)
    {
        syncState = SyncState::edited;
        juce::MessageManager::callAsync ([weak = juce::WeakReference<SynthController> (this)]
        {
            if (weak != nullptr)
                weak->sendChangeMessage();
        });
    }
}

Patch SynthController::getCurrentPatch() const
{
    // Only rewrite fields whose value changed: fields holding values the parameter
    // can't represent exactly (undocumented settings) stay byte-identical.
    auto p = basePatch;

    for (size_t i = 0; i < parameters.size(); ++i)
        if (auto v = sender.getValue ((int) i); v != device.getValue (basePatch, (int) i))
            device.setValue (p, (int) i, v);

    return p;
}

juce::String SynthController::getPatchName() const
{
    return device.getPatchName (basePatch);
}

void SynthController::setPatchName (const juce::String& newName)
{
    device.setPatchName (basePatch, newName);
    sendChangeMessage();
}

void SynthController::applyPatchToParameters (const Patch& patch)
{
    ++applyingFromSynth;

    for (size_t i = 0; i < parameters.size(); ++i)
    {
        auto value = device.getValue (patch, (int) i);
        auto& spec = device.getParams()[i];
        auto* p = parameters[i];
        auto plain = spec.kind == ParamSpec::Kind::choice ? value - spec.minValue : value;
        p->setValueNotifyingHost (p->convertTo0to1 ((float) plain));
        sender.setValue ((int) i, value, false);
    }

    --applyingFromSynth;
}

void SynthController::loadPatch (const Patch& patch, bool sendToSynthNow)
{
    if (! patch.isValid())
        return;

    basePatch = patch;
    applyPatchToParameters (patch);

    if (sendToSynthNow)
        sendToSynth();
    else
        setSyncState (SyncState::edited);
}

void SynthController::requestFromSynth()
{
    if (! active)
        return;

    sender.enqueue (device.createEditBufferRequest (getMidiSettings()));
    setSyncState (SyncState::waitingForSynth);
}

void SynthController::sendToSynth()
{
    sender.clearDirty();
    sender.enqueue (device.createEditBufferDump (getMidiSettings(), getCurrentPatch()));

    for (auto index : device.getParamsToRefreshAfterDump())
        sender.markDirty (index);

    setSyncState (SyncState::inSync);
}

void SynthController::writeToSlot (int slot)
{
    sender.enqueue (device.createSlotDump (getMidiSettings(), getCurrentPatch(), slot));
}

void SynthController::writePatchToSlot (const Patch& patch, int slot)
{
    sender.enqueue (device.createSlotDump (getMidiSettings(), patch, slot));
}

void SynthController::sendMidi (const juce::MidiMessage& m)
{
    sender.enqueue (m);
}

void SynthController::silence()
{
    // CC 120 All Sound Off. (The Kiwi-3P ignores CC 123 All Notes Off.)
    sendMidi (juce::MidiMessage::allSoundOff (getMidiSettings().channel));
}

void SynthController::requestSlot (int slot)
{
    sender.enqueue (device.createSlotRequest (getMidiSettings(), slot));
}

void SynthController::setPresetRef (PresetRef ref)
{
    presetRef = std::move (ref);
    sendChangeMessage();
}

void SynthController::setFrozen (bool shouldFreeze)
{
    sender.setFrozen (shouldFreeze);
    sendChangeMessage();
}

bool SynthController::isFrozen() const noexcept
{
    return sender.isFrozen();
}

DeviceDefinition::MidiSettings SynthController::getMidiSettings() const noexcept
{
    return { midiChannel.load(), deviceId.load() };
}

void SynthController::setMidiChannel (int channel)
{
    midiChannel = juce::jlimit (1, 16, channel);
    settings->get().setValue (settingsKey ("channel"), midiChannel.load());
    connectionChanged();
}

void SynthController::setDeviceId (int id)
{
    deviceId = juce::jlimit (0, 15, id);
    settings->get().setValue (settingsKey ("deviceId"), deviceId.load());
}

void SynthController::setMidiOutput (const juce::String& identifier)
{
    connection.setOutput (identifier);
    settings->get().setValue (settingsKey ("midiOut"), identifier);
}

void SynthController::setMidiInput (const juce::String& identifier)
{
    connection.setInput (identifier);
    settings->get().setValue (settingsKey ("midiIn"), identifier);
}

void SynthController::setSyncState (SyncState s)
{
    syncState = s;
    sendChangeMessage();
}

void SynthController::handleIncomingMessage (const juce::MidiMessage& m)
{
    if (! active)
        return;     // answers to another instance's requests

    auto incoming = device.parsePatchDump (getMidiSettings(), m);

    if (! incoming.has_value())
        return;

    if (incoming->slot >= 0)
    {
        if (onSlotReceived != nullptr)
            onSlotReceived (*incoming);

        return;
    }

    lastReceive = juce::Time::getCurrentTime();
    basePatch = incoming->patch;
    applyPatchToParameters (basePatch);
    setSyncState (SyncState::inSync);
}

//==============================================================================
juce::ValueTree SynthController::toValueTree() const
{
    juce::ValueTree v ("SynthController");
    v.setProperty ("patch", getCurrentPatch().toBase64(), nullptr);
    v.setProperty ("device", device.getDeviceId(), nullptr);
    v.setProperty ("presetFile", presetRef.file.getFullPathName(), nullptr);
    v.setProperty ("presetBank", presetRef.bank, nullptr);
    v.setProperty ("presetCategory", presetRef.category, nullptr);
    return v;
}

void SynthController::fromValueTree (const juce::ValueTree& v)
{
    if (v.getProperty ("device").toString() != device.getDeviceId())
        return;

    auto patch = Patch::fromBase64 (v.getProperty ("patch").toString());

    // Restore the sound (e.g. when a project is opened) and put it on the synth - now, or as
    // soon as the MIDI port is available and this instance controls the synth.
    if (patch.data.size() == (size_t) device.getPatchSize())
    {
        loadPatch (patch, false);
        pendingSend = true;
        startUsing();
        sendPendingSoundIfPossible();
    }

    auto file = v.getProperty ("presetFile").toString();
    setPresetRef ({ file.isNotEmpty() ? juce::File (file) : juce::File(),
                    v.getProperty ("presetBank").toString(), v.getProperty ("presetCategory").toString() });
}

} // namespace sso
