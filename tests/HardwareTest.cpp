#include <sso_kiwi3p/sso_kiwi3p.h>
#include <sso_kiwisix/sso_kiwisix.h>
#include <sso_hawk800/sso_hawk800.h>

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

/** TUNE against a connected synth: tunes for some seconds through SynthController, then
    ends tuning (which restores the synth's sound). Only touches the edit buffer.
    The ports are not stored in the settings.

    Usage: SSO_Tests --tune-test hawk800|kiwi3p|kiwisix "<in port part>" "<out port part>" <seconds> */
int runTuneTest (const juce::StringArray& args)
{
    auto i = args.indexOf ("--tune-test");
    auto deviceId = args[i + 1];
    std::unique_ptr<sso::DeviceDefinition> device;

    if (deviceId == "hawk800")      device = std::make_unique<sso::hawk800::Hawk800Device>();
    else if (deviceId == "kiwisix") device = std::make_unique<sso::kiwisix::KiwiSixDevice>();
    else                            device = std::make_unique<sso::kiwi3p::Kiwi3PDevice>();

    if (args.contains ("--print"))     // just the tune sound's bytes
    {
        auto tune = device->getTuneSound();

        for (auto b : tune->patch.data)
            std::cout << (int) b << " ";

        std::cout << std::endl;
        return 0;
    }

    DummyProcessor processor;
    juce::AudioProcessorValueTreeState state (processor, nullptr, "test", sso::SynthController::createParameterLayout (*device));
    sso::SynthController controller (*device, state);
    auto fake = args.contains ("--fake");      // dummy ports: shows what would be sent
    controller.getConnection().setInput (fake ? juce::String() : findPort (juce::MidiInput::getAvailableDevices(), args[i + 2]));
    controller.getConnection().setOutput (fake ? juce::String ("SSO-TEST-PORT-fake") : findPort (juce::MidiOutput::getAvailableDevices(), args[i + 3]));
    auto seconds = juce::jmax (1, args[i + 4].getIntValue());

    if (! fake && (! controller.getConnection().isOutputOpen() || ! controller.getConnection().isInputOpen()))
    {
        std::cout << "ports not open" << std::endl;
        return 1;
    }

    auto t0 = juce::Time::getMillisecondCounterHiRes();
    controller.getConnection().onMessageSent = [t0] (const juce::MidiMessage& m)
    {
        auto hex = juce::String::toHexString (m.getRawData(), juce::jmin (8, m.getRawDataSize()));
        std::cout << "  sent at " << juce::roundToInt (juce::Time::getMillisecondCounterHiRes() - t0) << " ms: "
                  << hex << (m.getRawDataSize() > 8 ? " ..." : "") << std::endl;
    };

    auto handler = controller.getConnection().onMessageReceived;
    controller.getConnection().onMessageReceived = [t0, handler] (const juce::MidiMessage& m)
    {
        if (m.getRawDataSize() > 1 || *m.getRawData() < 0xf8)
            std::cout << "  received at " << juce::roundToInt (juce::Time::getMillisecondCounterHiRes() - t0) << " ms: "
                      << juce::String::toHexString (m.getRawData(), juce::jmin (8, m.getRawDataSize()))
                      << (m.getRawDataSize() > 8 ? " ..." : "") << std::endl;
        handler (m);
    };

    controller.startUsing();
    pump (300);
    std::cout << "channel " << controller.getMidiSettings().channel << ", active " << controller.isActive() << std::endl;

    auto start = juce::Time::getMillisecondCounterHiRes();
    auto elapsed = [start] { return juce::roundToInt (juce::Time::getMillisecondCounterHiRes() - start); };
    controller.setTuning (true);

    while (! controller.isTuneSounding() && elapsed() < 2000)
        pump (5);

    std::cout << "tune note at " << elapsed() << " ms" << std::endl;
    pump (seconds * 1000);
    controller.setTuning (false);
    std::cout << "restored at " << elapsed() << " ms" << std::endl;
    pump (800);
    return 0;
}
