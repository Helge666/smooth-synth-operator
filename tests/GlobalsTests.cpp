#include <sso_kiwi3p/sso_kiwi3p.h>

namespace
{
    struct GlobalsDummyProcessor final : public juce::AudioProcessor
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

    // Globals as read from the JX-3P on 2026-10-11: stored in the synth (fine tune 0, arp on ...)
    // and as the Kiwi Patch Editor used to set them.
    const std::vector<uint8_t> storedInSynth { 0, 32, 5, 2, 2, 2, 5, 115, 5, 13, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0 };
    const std::vector<uint8_t> patchEditor   { 0, 64, 0, 0, 3, 0, 0, 50, 5, 2, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 63 };

    juce::MidiMessage globalsReply (const std::vector<uint8_t>& data24)
    {
        std::vector<uint8_t> d { 0x00, 0x21, 0x16, 0x60, 0x01, 0x00, 0x02 };
        d.insert (d.end(), data24.begin(), data24.end());
        d.resize (7 + 48, 0);
        return juce::MidiMessage::createSysExMessage (d.data(), (int) d.size());
    }
}

class GlobalsTests final : public juce::UnitTest
{
public:
    GlobalsTests() : juce::UnitTest ("Synth globals", "SSO") {}

    void runTest() override
    {
        sso::kiwi3p::Kiwi3PDevice device;
        sso::DeviceDefinition::MidiSettings midi;
        auto& globals = device.getGlobalParams();

        beginTest ("Kiwi-3P globals: musical only, no overlapping bits");
        {
            std::map<int, int> used;

            for (auto& g : globals)
            {
                expect (g.offset < 0x0a || g.offset > 0x16, g.id + " touches MIDI/system globals");
                auto mask = ((1 << g.bits) - 1) << g.shift;
                expect ((used[g.offset] & mask) == 0, g.id + " overlaps");
                used[g.offset] |= mask;
            }
        }

        beginTest ("Defaults are the Patch Editor's values; both dumps decode");
        {
            auto pe = device.parseGlobalsDump (midi, globalsReply (patchEditor));
            auto synth = device.parseGlobalsDump (midi, globalsReply (storedInSynth));
            expect (pe.has_value() && synth.has_value());

            for (int i = 0; i < (int) globals.size(); ++i)
                expectEquals (device.getGlobalValue (*pe, i), globals[(size_t) i].defaultValue, globals[(size_t) i].id);

            auto get = [&] (const char* id) { return device.getGlobalValue (*synth, device.indexOfGlobal (id)); };
            expectEquals (get ("g_fine_tune"), 0);
            expectEquals (get ("g_arp_on"), 1);
            expectEquals (get ("g_arp_direction"), 2);      // up & down
            expectEquals (get ("g_velocity"), 32);
            expectEquals (globals[(size_t) device.indexOfGlobal ("g_fine_tune")].toText (0), juce::String ("-200 ct"));
        }

        beginTest ("Global messages rebuild exactly the dump bytes ($10 offset value)");
        {
            auto synth = *device.parseGlobalsDump (midi, globalsReply (storedInSynth));
            auto messages = device.createGlobalMessages (midi, [&] (int i) { return device.getGlobalValue (synth, i); });
            std::set<int> offsets;

            for (auto& m : messages)
            {
                auto* d = m.getSysExData();
                expect (m.getSysExDataSize() == 9 && d[4] == 0x01 && d[6] == 0x10);
                expectEquals ((int) d[8], (int) storedInSynth[d[7]], "byte 0x" + juce::String::toHexString ((int) d[7]));
                offsets.insert (d[7]);
            }

            expect (offsets == std::set<int> { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x17 });
        }

        beginTest ("Controller sends its globals and takes over the synth's answer");
        {
            juce::SharedResourcePointer<sso::AppSettings> settings;
            auto& props = settings->get();
            std::map<juce::String, std::optional<juce::String>> saved;

            for (auto key : { "kiwi3p.sendGlobals", "kiwi3p.globals" })
                saved[key] = props.containsKey (key) ? std::optional<juce::String> (props.getValue (key)) : std::nullopt;

            const juce::ScopeGuard restore { [&]
            {
                for (auto& [key, value] : saved)
                    value.has_value() ? props.setValue (key, *value) : props.removeValue (key);
            } };

            props.setValue ("kiwi3p.sendGlobals", true);
            props.removeValue ("kiwi3p.globals");

            GlobalsDummyProcessor processor;
            juce::AudioProcessorValueTreeState state (processor, nullptr, "test", sso::SynthController::createParameterLayout (device));
            sso::SynthController controller (device, state);

            juce::CriticalSection lock;
            std::vector<juce::MidiMessage> sent;
            controller.getConnection().onMessageSent = [&] (const juce::MidiMessage& m) { const juce::ScopedLock sl (lock); sent.push_back (m); };

            auto port = "SSO-TEST-PORT-globals-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt());
            controller.getConnection().setOutput (port);
            controller.startUsing();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

            // (sending on connect needs a port that really opens - checked on the hardware)
            controller.sendGlobals();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (300);

            auto globalMessages = [&]
            {
                const juce::ScopedLock sl (lock);
                return (int) std::count_if (sent.begin(), sent.end(), [] (auto& m) { return m.isSysEx() && m.getSysExData()[6] == 0x10; });
            };

            expectEquals (globalMessages(), 11, "all global bytes sent");

            bool fineTuneCentred = false;
            {
                const juce::ScopedLock sl (lock);
                for (auto& m : sent)
                    if (m.isSysEx() && m.getSysExData()[6] == 0x10 && m.getSysExData()[7] == 0x17)
                        fineTuneCentred = m.getSysExData()[8] == 63;
            }
            expect (fineTuneCentred, "fine tune sent centred");

            // An answer from the synth replaces the stored values
            controller.requestGlobalsFromSynth();
            controller.getConnection().onMessageReceived (globalsReply (storedInSynth));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            expectEquals (controller.getGlobalValue (device.indexOfGlobal ("g_fine_tune")), 0);

            // Changing one value sends the globals again
            { const juce::ScopedLock sl (lock); sent.clear(); }
            controller.setGlobalValue (device.indexOfGlobal ("g_fine_tune"), 63);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
            expectEquals (globalMessages(), 11, "change not sent");
        }
    }
};

static GlobalsTests globalsTests;
