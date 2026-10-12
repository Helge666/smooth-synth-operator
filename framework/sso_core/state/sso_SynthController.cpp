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
    tuneSound = device.getTuneSound();

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
    connection.onLoopDetected = [this]
    {
        registry->warn ("MIDI loop detected on '" + connection.getOutputId() + "' - sending stopped");
        sendChangeMessage();
    };

    auto& props = settings->get();
    midiChannel = props.getIntValue (settingsKey ("channel"), 1);
    deviceId = props.getIntValue (settingsKey ("deviceId"), 0);

    for (auto& spec : device.getGlobalParams())
        globalValues.push_back (spec.defaultValue);

    if (auto* stored = juce::JSON::parse (props.getValue (settingsKey ("globals"))).getDynamicObject())
        for (size_t i = 0; i < globalValues.size(); ++i)
            if (auto& spec = device.getGlobalParams()[i]; stored->hasProperty (spec.id))
                globalValues[i] = juce::jlimit (spec.minValue, spec.maxValue, (int) stored->getProperty (spec.id));

    tuneLevelDb = juce::jlimit (minTuneLevelDb, maxTuneLevelDb,
                                (float) props.getDoubleValue (settingsKey ("tuneLevelDb"), defaultTuneLevelDb));

    connection.setOutput (props.getValue (settingsKey ("midiOut")));
    connection.setInput (props.getValue (settingsKey ("midiIn")));

    registry->log ("controller created, settings " + props.getFile().getFullPathName()
                   + (props.getFile().existsAsFile() ? "" : " (missing)")
                   + ", midiOut '" + props.getValue (settingsKey ("midiOut")) + "'"
                   + ", output open: " + (connection.isOutputOpen() ? "yes" : "no"));

    registeredKey = getOwnershipKey();
    registered = true;
    registry->add (*this);
    startTimer (1000);
}

SynthController::~SynthController()
{
    stopTimer();

    // Closing the app or project while tuning: the synth gets its sound back. Sent directly,
    // the paced queue ends with this object.
    if (std::exchange (tuneLoaded, false) && active.load())
    {
        sender.setMuted (true);
        auto channel = getMidiSettings().channel;
        auto send = [this] (const juce::MidiMessage& m) { connection.send (m); };

        if (tuneSounding.exchange (false))
            send (juce::MidiMessage::noteOff (channel, tuneSound->note));

        send (juce::MidiMessage::allSoundOff (channel));
        sendSoundToEditBuffer (tuneRestore, send);
    }

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
    auto wasActive = active.load();

    // Another instance takes over while tuning: release the note, it sends its own sound
    if (! isOwner && tuning.load())
    {
        if (tuneSounding.exchange (false))
            connection.send (juce::MidiMessage::noteOff (getMidiSettings().channel, tuneSound->note));

        tuning = false;
        tuneLoaded = false;
        capturingForTune = false;
        ++tuneGeneration;
    }

    active = isOwner;
    sender.setMuted (! active);

    if (active != wasActive)
        registry->log ("  " + description + ": " + (active ? "active" : "inactive") + (takenOver ? " (taken over)" : ""));

    if (active && ! wasActive && takenOver)
        pendingSend = true;     // the user wants this instance's sound on the synth

    if (! active)
        globalsSentTo = {};     // whoever controls the synth now sends its own

    sendGlobalsIfDue();
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

    sendGlobalsIfDue();
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

void SynthController::sendGlobalsIfDue()
{
    // only once the registry decided whether this instance controls the synth
    if (! registered || device.getGlobalParams().empty() || ! getSendGlobalsOnConnect() || ! active || ! connection.isOutputOpen())
        return;

    if (globalsSentTo == connection.getOutputId())
        return;

    globalsSentTo = connection.getOutputId();
    sendGlobals();
}

void SynthController::sendGlobals()
{
    if (! active)
        return;

    for (auto& m : device.createGlobalMessages (getMidiSettings(), [this] (int i) { return globalValues[(size_t) i]; }))
        sender.enqueue (m);
}

int SynthController::getGlobalValue (int index) const
{
    return juce::isPositiveAndBelow (index, (int) globalValues.size()) ? globalValues[(size_t) index] : 0;
}

void SynthController::setGlobalValue (int index, int value)
{
    if (! juce::isPositiveAndBelow (index, (int) globalValues.size()))
        return;

    auto& spec = device.getGlobalParams()[(size_t) index];
    globalValues[(size_t) index] = juce::jlimit (spec.minValue, spec.maxValue, value);
    storeGlobals();
    sendGlobals();      // all of them - a dozen short messages
    sendChangeMessage();
}

void SynthController::storeGlobals()
{
    auto* obj = new juce::DynamicObject();

    for (size_t i = 0; i < globalValues.size(); ++i)
        obj->setProperty (device.getGlobalParams()[i].id, globalValues[i]);

    settings->get().setValue (settingsKey ("globals"), juce::JSON::toString (juce::var (obj), true));
}

bool SynthController::getSendGlobalsOnConnect() const
{
    return settings->get().getBoolValue (settingsKey ("sendGlobals"), false);
}

void SynthController::setSendGlobalsOnConnect (bool shouldSend)
{
    settings->get().setValue (settingsKey ("sendGlobals"), shouldSend);
    globalsSentTo = {};
    sendGlobalsIfDue();
    sendChangeMessage();
}

void SynthController::requestGlobalsFromSynth()
{
    if (active && ! device.getGlobalParams().empty())
    {
        globalsRequested = true;
        sender.enqueue (device.createGlobalsRequest (getMidiSettings()));
    }
}

//==============================================================================
static constexpr double quietBeforeProbeMs = 10000.0, probeTimeoutMs = 1500.0;

void SynthController::probeSynth()
{
    // A harmless read-only request; any answer marks the synth as present
    auto probe = device.createGlobalsRequest (getMidiSettings());

    if (probe.getRawDataSize() == 0 || ! active || ! connection.isOutputOpen() || ! connection.isInputOpen())
        return;

    probeSentMs = juce::Time::getMillisecondCounterHiRes();
    sender.enqueue (probe);
}

void SynthController::timerCallback()
{
    auto before = getPresence();
    auto now = juce::Time::getMillisecondCounterHiRes();
    auto heard = connection.getLastHeardMs();

    if (probeSentMs > 0 && heard >= probeSentMs)
    {
        probeSentMs = 0;                                    // answered
        probeUnanswered = false;
    }
    else if (probeSentMs > 0 && now - probeSentMs > probeTimeoutMs)
    {
        probeUnanswered = true;
    }

    // ask after 10 s of silence, and again every 10 s while there is no answer
    auto lastActivity = juce::jmax (heard, probeSentMs);

    if (now - lastActivity > quietBeforeProbeMs)
        probeSynth();

    if (getPresence() != before)
        sendChangeMessage();
}

SynthController::Presence SynthController::getPresence() const
{
    if (! connection.isOutputOpen() || ! connection.isInputOpen() || ! active)
        return Presence::unknown;

    auto now = juce::Time::getMillisecondCounterHiRes();

    if (now - connection.getLastHeardMs() < quietBeforeProbeMs + probeTimeoutMs)
        return Presence::present;

    if (probeUnanswered)
        return Presence::absent;

    return Presence::unknown;
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
    sender.setValue (index, value, ! fromSynth && ! tuning.load());     // edits while tuning: sent afterwards

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
    if (! active || tuning.load())
        return;

    sender.enqueue (device.createEditBufferRequest (getMidiSettings()));
    setSyncState (SyncState::waitingForSynth);
}

void SynthController::sendToSynth()
{
    if (tuning.load())
        endTuning (false);      // a new sound replaces the tune sound

    sender.clearDirty();
    sender.enqueue (device.createEditBufferDump (getMidiSettings(), getCurrentPatch()));

    for (auto index : device.getParamsToRefreshAfterDump())
        sender.markDirty (index);

    setSyncState (SyncState::inSync);
}

void SynthController::writeToSlot (int slot)
{
    writePatchToSlot (getCurrentPatch(), slot);
}

void SynthController::writePatchToSlot (const Patch& patch, int slot)
{
    synthBank = -1;     // a write always selects its bank: the user may have changed it on the panel
    selectBankFor (slot);
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
    selectBankFor (slot);
    sender.enqueue (device.createSlotRequest (getMidiSettings(), slot));
}

void SynthController::selectBankFor (int slot)
{
    auto bank = device.getSlotBank (slot);

    if (bank < 0 || bank == synthBank)
        return;

    sender.enqueue (device.createBankSelect (getMidiSettings(), bank));
    synthBank = bank;
}

void SynthController::beginSlotAccess()
{
    accessingSlots = true;
    synthBank = -1;     // select the bank again before the first access
}

void SynthController::endSlotAccess()
{
    accessingSlots = false;

    // Leave the synth on the bank the user had selected on its panel
    if (panelBank >= 0 && synthBank >= 0 && synthBank != panelBank)
    {
        sender.enqueue (device.createBankSelect (getMidiSettings(), panelBank));
        synthBank = panelBank;
    }
}

//==============================================================================
static constexpr int tuneCaptureTimeoutMs = 800;

void SynthController::setTuning (bool shouldTune)
{
    if (! shouldTune)
    {
        if (tuning.load())
            endTuning (true);

        return;
    }

    if (tuning.load() || ! tuneSound.has_value() || ! active.load())
        return;

    tuning = true;
    tuneAppPatch = getCurrentPatch();
    tuneRestore = tuneAppPatch;         // unless the synth reports what it plays
    auto generation = ++tuneGeneration;

    // Ask the synth what it plays; the tune sound follows the answer (or a timeout)
    capturingForTune = true;
    sender.enqueue (device.createEditBufferRequest (getMidiSettings()));

    juce::Timer::callAfterDelay (tuneCaptureTimeoutMs, [weak = juce::WeakReference<SynthController> (this), generation]
    {
        if (weak != nullptr)
            weak->startTuneSound (generation);
    });

    sendChangeMessage();
}

void SynthController::setTuneLevelDb (float db)
{
    tuneLevelDb = juce::jlimit (minTuneLevelDb, maxTuneLevelDb, db);
    settings->get().setValue (settingsKey ("tuneLevelDb"), (double) tuneLevelDb.load());
}

void SynthController::startTuneSound (int generation)
{
    if (generation != tuneGeneration || ! tuning.load() || tuneLoaded || ! active.load())
        return;

    capturingForTune = false;
    tuneLoaded = true;
    silence();

    // The note follows when the synth has taken the sound: the dump's time on the wire plus a pause
    double wireMs = 0;
    sendSoundToEditBuffer (tuneSound->patch, [this, &wireMs] (const juce::MidiMessage& m)
    {
        wireMs += m.getRawDataSize() * 10.0 * 1000.0 / 31250.0;
        sender.enqueue (m);
    });

    juce::Timer::callAfterDelay (juce::roundToInt (wireMs + device.getSettleTimeMs()),
                                 [weak = juce::WeakReference<SynthController> (this), generation]
    {
        if (weak != nullptr)
            weak->playTuneNote (generation);
    });
}

void SynthController::playTuneNote (int generation)
{
    if (generation != tuneGeneration || ! tuning.load() || ! tuneLoaded || ! active.load())
        return;

    sender.enqueue (juce::MidiMessage::noteOn (getMidiSettings().channel, tuneSound->note, (juce::uint8) 100));
    tuneSounding = true;
    sendChangeMessage();
}

void SynthController::endTuning (bool restore)
{
    tuning = false;
    capturingForTune = false;
    ++tuneGeneration;

    if (std::exchange (tuneLoaded, false))
    {
        if (tuneSounding.exchange (false))
            sender.enqueue (juce::MidiMessage::noteOff (getMidiSettings().channel, tuneSound->note));

        silence();

        if (restore)
        {
            sendSoundToEditBuffer (tuneRestore, [this] (const juce::MidiMessage& m) { sender.enqueue (m); });

            // edits made meanwhile (e.g. host automation) follow the restored sound
            for (int i = 0; i < (int) parameters.size(); ++i)
                if (sender.getValue (i) != device.getValue (tuneAppPatch, i))
                    sender.markDirty (i);
        }
    }

    sendChangeMessage();
}

void SynthController::sendSoundToEditBuffer (const Patch& patch, const std::function<void (const juce::MidiMessage&)>& send)
{
    auto midiSettings = getMidiSettings();
    send (device.createEditBufferDump (midiSettings, patch));

    // values the hardware does not take from a dump
    auto valueOf = [this, &patch] (int i) { return device.getValue (patch, i); };

    for (auto index : device.getParamsToRefreshAfterDump())
        for (auto& m : device.createParamMessages (midiSettings, index, valueOf))
            send (m);
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

bool SynthController::getForwardNotes() const          { return settings->get().getBoolValue (settingsKey ("forwardNotes"), true); }
void SynthController::setForwardNotes (bool shouldForward) { settings->get().setValue (settingsKey ("forwardNotes"), shouldForward); sendChangeMessage(); }
double SynthController::getNoteOffsetMs() const         { return settings->get().getDoubleValue (settingsKey ("noteOffsetMs"), 0.0); }
void SynthController::setNoteOffsetMs (double ms)       { settings->get().setValue (settingsKey ("noteOffsetMs"), ms); sendChangeMessage(); }

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

    if (auto globals = device.parseGlobalsDump (getMidiSettings(), m))
    {
        if (auto bank = device.getSelectedBank (*globals); bank >= 0 && ! accessingSlots)
            panelBank = synthBank = bank;

        if (! std::exchange (globalsRequested, false))
            return;     // answer to a presence probe

        for (size_t i = 0; i < globalValues.size(); ++i)
            globalValues[i] = device.getGlobalValue (*globals, (int) i);

        storeGlobals();
        sendChangeMessage();

        if (onGlobalsReceived != nullptr)
            onGlobalsReceived();

        return;
    }

    auto incoming = device.parsePatchDump (getMidiSettings(), m);

    if (! incoming.has_value())
        return;

    if (incoming->slot >= 0)
    {
        // Bank-relative slot numbers: in the bank we selected
        if (device.getSlotBank (0) >= 0)
        {
            if (synthBank < 0)
                return;

            auto layout = device.getSlotLayout();
            incoming->slot += synthBank * layout.groupsPerBank * layout.slotsPerGroup;
        }

        if (onSlotReceived != nullptr)
            onSlotReceived (*incoming);

        return;
    }

    if (tuning.load())
    {
        // The answer to the request before tuning is the sound to restore; ignore anything later
        if (std::exchange (capturingForTune, false))
        {
            tuneRestore = incoming->patch;
            startTuneSound (tuneGeneration);
        }

        return;
    }

    lastReceive = juce::Time::getCurrentTime();

    // A synth without names sends none: keep ours while it still is the same sound
    if (! device.storesPatchNames() && device.asStored (incoming->patch) == device.asStored (getCurrentPatch()))
        device.setPatchName (incoming->patch, getPatchName());

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
