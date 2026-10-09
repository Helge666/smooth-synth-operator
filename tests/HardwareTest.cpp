#include <sso_kiwi3p/sso_kiwi3p.h>

/** End-to-end check of SynthController against a connected Kiwi-3P.
    Only touches the edit buffer and restores it at the end.

    Usage: SSO_Tests --hardware "MIDISPORT 4x4 Anniv) 2"   (substring of the port names)
*/
namespace
{
    struct DummyProcessor final : public juce::AudioProcessor
    {
        const juce::String getName() const override { return "test"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return true; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    void pump (int ms)  { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

    juce::String findPort (const juce::Array<juce::MidiDeviceInfo>& devices, const juce::String& part)
    {
        for (auto& d : devices)
            if (d.name.contains (part))
                return d.identifier;

        return {};
    }
}

/** Plays the TEST phrase once through SynthController (SSO_Tests --play-test "<port part>"). */
int runPlayTest (const juce::String& portPart)
{
    sso::kiwi3p::Kiwi3PDevice device;
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState state (processor, nullptr, "test", sso::SynthController::createParameterLayout (device));
    sso::SynthController controller (device, state);
    controller.setMidiOutput (findPort (juce::MidiOutput::getAvailableDevices(), portPart));
    controller.startUsing();

    sso::TestNotePlayer player (controller);
    player.play();
    auto start = juce::Time::getMillisecondCounterHiRes();

    while (player.isPlaying() && juce::Time::getMillisecondCounterHiRes() - start < 15000)
        pump (50);

    pump (200);
    std::cout << "played test phrase in " << juce::roundToInt (juce::Time::getMillisecondCounterHiRes() - start) << " ms" << std::endl;
    return player.isPlaying() ? 1 : 0;
}

int runHardwareTest (const juce::String& portPart)
{
    sso::kiwi3p::Kiwi3PDevice device;
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState state (processor, nullptr, "test", sso::SynthController::createParameterLayout (device));
    sso::SynthController controller (device, state);
    int failures = 0;

    auto check = [&failures] (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  ok    " : "  FAIL  ") << what << std::endl;
        if (! ok) ++failures;
    };

    auto outId = findPort (juce::MidiOutput::getAvailableDevices(), portPart);
    auto inId  = findPort (juce::MidiInput::getAvailableDevices(), portPart);
    controller.setMidiOutput (outId);
    controller.setMidiInput (inId);
    controller.startUsing();
    check (controller.getConnection().isOutputOpen() && controller.getConnection().isInputOpen(), "ports open (" + portPart + ")");

    auto fetch = [&]
    {
        controller.requestFromSynth();

        for (int i = 0; i < 40 && controller.getSyncState() != sso::SynthController::SyncState::inSync; ++i)
            pump (50);

        return controller.getSyncState() == sso::SynthController::SyncState::inSync;
    };

    auto setParam = [&] (const char* id, int value)
    {
        auto* p = state.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 ((float) value));
    };

    auto valueOnSynth = [&] (const char* id) { return device.getValue (controller.getCurrentPatch(), device.indexOf (id)); };

    check (fetch(), "edit buffer received");
    auto original = controller.getCurrentPatch();
    std::cout << "  sound on synth: \"" << controller.getPatchName() << "\"" << std::endl;

    // CC path
    setParam ("cutoff", 30);
    pump (200);
    check (fetch() && valueOnSynth ("cutoff") == 30, "cutoff via CC");

    // Choice with remapped order (Square is stored as 2, shown as index 4)
    setParam ("lfo1_wave", 4);
    pump (200);
    check (fetch() && valueOnSynth ("lfo1_wave") == 4, "LFO 1 wave = Square via CC");

    // SysEx single-parameter path (packed bits)
    setParam ("mw_dco_lfo", 3);
    setParam ("mw_mix", 1);
    pump (200);
    check (fetch() && valueOnSynth ("mw_dco_lfo") == 3 && valueOnSynth ("mw_mix") == 1, "mod wheel routing via SysEx");

    // Freeze holds changes back until released
    auto resoBefore = valueOnSynth ("resonance");
    controller.setFrozen (true);
    setParam ("resonance", resoBefore == 90 ? 91 : 90);
    pump (300);
    check (fetch() && valueOnSynth ("resonance") == resoBefore, "freeze holds the change");
    // fetching replaced the parameters with the synth state - set again and release
    setParam ("resonance", resoBefore == 90 ? 91 : 90);
    controller.setFrozen (false);
    pump (300);
    check (fetch() && valueOnSynth ("resonance") == (resoBefore == 90 ? 91 : 90), "change sent after freeze released");

    // Full patch send + name
    auto renamed = original;
    device.setPatchName (renamed, "SSO Roundtrip");
    controller.loadPatch (renamed, true);
    pump (400);
    check (fetch() && controller.getCurrentPatch() == renamed, "edit buffer dump round trip incl. name");

    // Restore
    controller.loadPatch (original, true);
    pump (400);
    check (fetch() && controller.getCurrentPatch() == original, "original sound restored");

    std::cout << (failures == 0 ? "HARDWARE TEST PASSED" : "HARDWARE TEST FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
