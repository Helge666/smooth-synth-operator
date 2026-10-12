namespace sso
{

ProgrammerProcessor::ProgrammerProcessor (std::unique_ptr<const DeviceDefinition> d, const juce::String& stateType,
                                          const juce::String& name)
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      pluginName (name),
      ownedDevice (std::move (d)),
      device (*ownedDevice),
      parameters (*this, nullptr, stateType, SynthController::createParameterLayout (device)),
      controller (device, parameters)
{
    controller.addChangeListener (this);
    changeListenerCallback (nullptr);

    juce::MessageManager::callAsync ([this, alive = aliveFlag]
    {
        if (*alive)
            controller.setInstanceDescription (wrapperType == wrapperType_Standalone
                                                   ? juce::String ("the SSO standalone app")
                                                   : juce::String (juce::PluginHostType().getHostDescription()) + " (plugin)");
    });
}

void ProgrammerProcessor::updateTrackProperties (const TrackProperties& properties)
{
    // May be called on any thread
    auto trackName = properties.name.value_or (juce::String());

    juce::MessageManager::callAsync ([this, alive = aliveFlag, trackName]
    {
        if (*alive && trackName.isNotEmpty())
            controller.setInstanceDescription (juce::String (juce::PluginHostType().getHostDescription())
                                               + " - track \"" + trackName + "\"");
    });
}

ProgrammerProcessor::~ProgrammerProcessor()
{
    *aliveFlag = false;
    controller.removeChangeListener (this);
}

void ProgrammerProcessor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // forwarding settings are read on the audio thread
    forwardNotes = controller.getForwardNotes();
    noteOffsetMs = controller.getNoteOffsetMs();
}

void ProgrammerProcessor::prepareToPlay (double newSampleRate, int maximumBlockSize)
{
    forwarder.prepare (newSampleRate, maximumBlockSize);
    tuneTone.prepare (newSampleRate, controller.getTuneWave());

    juce::MessageManager::callAsync ([this, alive = aliveFlag]
    {
        if (*alive)
            controller.startUsing();
    });
}

void ProgrammerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    auto playing = false;

    if (auto* hostPlayHead = getPlayHead())
        if (auto position = hostPlayHead->getPosition())
            playing = position->getIsPlaying();

    forwarder.setEnabled (forwardNotes.load() && ! controller.isTuning());   // the synth holds the tune note
    forwarder.setUserOffsetMs (noteOffsetMs.load());
    forwarder.process (midi, buffer.getNumSamples(), playing);

    midi.clear();       // the notes go straight to the synth
    buffer.clear();
    tuneTone.process (buffer, controller.isTuneSounding(), controller.getTuneLevelDb());
}

void ProgrammerProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    forwarder.releaseHeldNotes();
    midi.clear();
    buffer.clear();
}

void ProgrammerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.removeChild (state.getChildWithName ("SynthController"), nullptr);
    state.appendChild (controller.toValueTree(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ProgrammerProcessor::setStateInformation (const void* data, int sizeInBytes)
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

} // namespace sso
