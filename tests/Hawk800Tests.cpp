#include <sso_hawk800/sso_hawk800.h>

using sso::hawk800::Hawk800Device;

namespace
{
    struct HawkDummyProcessor final : public juce::AudioProcessor
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

    bool sameBytes (const juce::MidiMessage& m, std::initializer_list<uint8_t> bytes)
    {
        std::vector<uint8_t> v (bytes);
        return m.getRawDataSize() == (int) v.size() && std::memcmp (m.getRawData(), v.data(), v.size()) == 0;
    }

    juce::MidiMessage sysEx (std::vector<uint8_t> body)
    {
        return juce::MidiMessage::createSysExMessage (body.data(), (int) body.size());
    }

    juce::MidiMessage globalsReply (int bank)
    {
        std::vector<uint8_t> body { 0x42, 0x21, 0x08 };
        std::vector<uint8_t> globals (64, 0);
        globals[2] = (uint8_t) bank;

        for (auto g : globals)
        {
            body.push_back ((uint8_t) (g >> 4));
            body.push_back ((uint8_t) (g & 15));
        }

        return sysEx (body);
    }

    std::vector<std::vector<sso::DeviceDefinition::IncomingPatch>> factoryBanks (const Hawk800Device& device)
    {
        std::vector<std::vector<sso::DeviceDefinition::IncomingPatch>> banks;
       #ifdef SSO_LOCAL_HAWK_FACTORY
        for (int b = 1; b <= 4; ++b)
            for (auto& m : sso::PresetLibrary::readSyxFile (juce::File (SSO_LOCAL_HAWK_FACTORY).getChildFile ("Bank" + juce::String (b) + ".syx")))
                if (auto bulk = device.parseBulkDump ({}, m); ! bulk.empty())
                    banks.push_back (bulk);
       #endif
        return banks;
    }
}

class Hawk800DeviceTests final : public juce::UnitTest
{
public:
    Hawk800DeviceTests() : juce::UnitTest ("HAWK-800 device definition", "SSO") {}

    void runTest() override
    {
        Hawk800Device device;
        sso::DeviceDefinition::MidiSettings midi;
        auto& params = device.getParams();

        beginTest ("Parameter table: one byte per parameter, values in range");
        {
            juce::StringArray ids;
            std::set<int> offsets;

            for (auto& p : params)
            {
                expect (! ids.contains (p.id), "duplicate id " + p.id);
                ids.add (p.id);
                expect (offsets.insert (p.offset).second, "offset used twice: " + p.id);
                expect (juce::isPositiveAndBelow (p.offset, Hawk800Device::synthPatchSize), p.id);
                expect (p.defaultValue >= p.minValue && p.defaultValue <= p.maxValue, p.id);
            }

            // P1 11-88 = bytes 0-63, P2 11-88 = 64-127
            expectEquals (params[(size_t) device.indexOf ("cutoff")].offset, 24);         // P1 41
            expectEquals (params[(size_t) device.indexOf ("resonance")].offset, 96);      // P2 51
            expectEquals (params[(size_t) device.indexOf ("key_assign")].offset, 127);    // P2 88
            expectEquals (params[(size_t) device.indexOf ("eg3_release")].offset, 53);    // P1 76
        }

        beginTest ("Messages");
        {
            auto cutoff = device.indexOf ("cutoff");
            auto messages = device.createParamMessages (midi, cutoff, [] (int) { return 69; });
            expect (messages.size() == 1 && sameBytes (messages[0], { 0xf0, 0x42, 0x21, 0x0f, 24, 69, 0xf7 }));

            auto polarity = device.indexOf ("eg3_polarity");
            messages = device.createParamMessages (midi, polarity, [] (int) { return 1; });
            expect (sameBytes (messages[0], { 0xf0, 0x42, 0x21, 0x0f, 26, 1, 0xf7 }), "+ is stored as 1");

            expect (sameBytes (device.createEditBufferRequest (midi), { 0xf0, 0x42, 0x21, 0x07, 0x40, 0xf7 }));
            expect (sameBytes (device.createSlotRequest (midi, 3 * 64 + 17), { 0xf0, 0x42, 0x21, 0x07, 17, 0xf7 }));
            expect (sameBytes (device.createBankSelect (midi, 2), { 0xf0, 0x42, 0x21, 0x0e, 2, 0xf7 }));
            expect (sameBytes (device.createGlobalsRequest (midi), { 0xf0, 0x42, 0x21, 0x0a, 2, 0xf7 }));
            expectEquals (device.getSlotName (64 + 0), juce::String ("2-11"));
            expectEquals (device.getSlotName (3 * 64 + 63), juce::String ("4-88"));
            expectEquals (device.getSlotBank (130), 2);
        }

        beginTest ("Dumps round-trip; the name never reaches the synth");
        {
            auto p = device.createInitPatch();
            expectEquals (device.getPatchName (p), juce::String ("Init"));
            expectEquals ((int) p.data.size(), device.getPatchSize());

            for (int i = 0; i < Hawk800Device::synthPatchSize; ++i)
                p.data[(size_t) i] = (uint8_t) ((i * 37) & 0x7f);

            device.setPatchName (p, "Percussive Pipe Organ A");
            auto dump = device.createEditBufferDump (midi, p);
            expectEquals (dump.getSysExDataSize(), 4 + 256);

            auto parsed = device.parsePatchDump (midi, dump);
            expect (parsed.has_value() && parsed->slot == -1);
            expect (parsed->patch == device.asStored (p), "synth bytes equal, name empty");
            expectEquals (device.getPatchName (parsed->patch), juce::String());
            expectEquals (device.getPatchName (p), juce::String ("Percussive Pipe Organ A"));

            auto slot = device.parsePatchDump (midi, device.createSlotDump (midi, p, 2 * 64 + 40));
            expect (slot.has_value() && slot->slot == 40, "slot within the bank");

            // the synth's acknowledgements are no dumps
            expect (! device.parsePatchDump (midi, sysEx ({ 0x42, 0x21, 0x7e, 0x06, 0x40 })).has_value());
            expect (! device.parsePatchDump (midi, sysEx ({ 0x42, 0x21, 0x7e, 0x0e, 0x01 })).has_value());

            auto globals = device.parseGlobalsDump (midi, globalsReply (1));
            expect (globals.has_value() && device.getSelectedBank (*globals) == 1);
        }

        auto banks = factoryBanks (device);

        if (banks.size() == 4)
        {
            beginTest ("Factory banks: 4 x 64 sounds, every byte survives the parameters");
            {
                for (auto& bank : banks)
                {
                    expectEquals ((int) bank.size(), 64);

                    for (auto& incoming : bank)
                    {
                        // Writing every parameter's (clamped) value back changes nothing - except values
                        // outside the parameter's range, which the controller never rewrites unless edited
                        auto copy = incoming.patch;

                        for (int i = 0; i < (int) params.size(); ++i)
                            if (incoming.patch.data[(size_t) params[(size_t) i].offset] <= params[(size_t) i].maxValue)
                                device.setValue (copy, i, device.getValue (incoming.patch, i));

                        expect (copy == incoming.patch);
                    }
                }

                int outOfRange = 0;

                for (auto& bank : banks)
                    for (auto& incoming : bank)
                        for (auto& spec : params)
                            outOfRange += incoming.patch.data[(size_t) spec.offset] > spec.maxValue ? 1 : 0;

                expect (outOfRange < 70, "factory values outside the ranges: " + juce::String (outOfRange));
            }

            beginTest ("Factory import: Korg names, variations, Sound b-nn, no copies");
            {
                auto root = juce::File::createTempFile ("hawk-lib");
                root.createDirectory();
                const juce::ScopeGuard cleanup { [&] { root.deleteRecursively(); } };

                sso::PresetLibrary library (device, root);
                auto result = sso::hawk800::importFactoryBanks (library, device, juce::File (SSO_LOCAL_HAWK_FACTORY));
                logMessage ("imported " + juce::String (result.imported));
                expect (result.imported > 190 && result.imported < 210);

                std::map<juce::String, const sso::Preset*> byName;
                std::vector<sso::Patch> sounds;

                for (auto& p : library.getPresets())
                {
                    expect (byName.emplace (p.name, &p).second, "name twice: " + p.name);
                    expectEquals (device.getPatchName (p.patch), p.name);
                    sounds.push_back (device.asStored (p.patch));
                }

                for (size_t i = 0; i < sounds.size(); ++i)
                    for (size_t j = i + 1; j < sounds.size(); ++j)
                        expect (! (sounds[i] == sounds[j]), "same sound twice");

                expectEquals (byName["Vibes A"]->category, juce::String ("Bells"));
                expectEquals (byName["Helicopter"]->category, juce::String ("FX"));

                for (auto* expectedName : { "Init", "Strings", "Acoustic Piano", "UFO Swoop", "Helicopter A", "Vibes A", "Sound 3-11", "Sound 1-11" })
                    expect (byName.count (expectedName) == 1, juce::String ("missing ") + expectedName);

                // the variation has the original's P1 parameters
                auto& heli = byName["Helicopter"]->patch.data;
                auto& heliA = byName["Helicopter A"]->patch.data;
                expect (std::equal (heli.begin(), heli.begin() + 64, heliA.begin()));
                expect (! std::equal (heli.begin(), heli.begin() + 128, heliA.begin()));

                // a second import adds nothing
                expectEquals (sso::hawk800::importFactoryBanks (library, device, juce::File (SSO_LOCAL_HAWK_FACTORY)).imported, 0);
            }
        }

        beginTest ("Generator: values in range, sounds playable");
        {
            auto generator = sso::hawk800::createGenerator (device);
            juce::Random random (800);

            for (auto& kind : generator->getCategories())
            {
                for (int n = 0; n < 40; ++n)
                {
                    auto p = generator->generate (kind, device.createInitPatch(), {}, random);
                    auto get = [&] (const char* id) { return device.getValue (p, device.indexOf (id)); };

                    for (auto& spec : params)
                        expect (p.data[(size_t) spec.offset] <= spec.maxValue, kind + " " + spec.id);

                    expect (get ("dco1_level") >= 8 && get ("dco1_harmonics") != 15, kind + " DCO 1 silent");
                    expect (get ("eg1_sustain") >= 8 || get ("eg1_decay") >= 12, kind + " envelope too short");
                    expect (get ("cutoff") + (get ("eg3_polarity") == 1 ? get ("eg3_int") * 5 : 0) >= 20, kind + " filter closed");
                    expect (device.getPatchName (p).isNotEmpty());
                }
            }
        }

        beginTest ("Controller: bank-relative slots, panel bank restored");
        {
            HawkDummyProcessor processor;
            juce::AudioProcessorValueTreeState state (processor, nullptr, "test", sso::SynthController::createParameterLayout (device));
            sso::SynthController controller (device, state);

            juce::CriticalSection lock;
            std::vector<juce::MidiMessage> sent;
            controller.getConnection().onMessageSent = [&] (const juce::MidiMessage& m) { const juce::ScopedLock sl (lock); sent.push_back (m); };
            controller.getConnection().setOutput ("SSO-TEST-PORT-hawk-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()));
            controller.startUsing();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

            // the synth reports bank 2 (raw 1) selected on its panel
            controller.getConnection().onMessageReceived (globalsReply (1));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            { const juce::ScopedLock sl (lock); sent.clear(); }

            sso::BankReader reader (controller);
            std::optional<sso::DeviceDefinition::IncomingPatch> got;
            bool done = false;
            reader.start ({ 2 * 64 + 22 }, nullptr, [&] (sso::BankReader::Result r, bool complete)
            {
                done = complete;
                if (! r.empty()) got = r.front();
            }, false);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);

            auto patch = device.createInitPatch();
            controller.getConnection().onMessageReceived (device.createSlotDump (midi, patch, 22));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);

            expect (done && got.has_value() && got->slot == 2 * 64 + 22, "slot mapped into bank 3");

            const juce::ScopedLock sl (lock);
            expect (sent.size() >= 3);

            if (sent.size() >= 3)
            {
                expect (sameBytes (sent[0], { 0xf0, 0x42, 0x21, 0x0e, 2, 0xf7 }), "bank 3 selected first");
                expect (sameBytes (sent[1], { 0xf0, 0x42, 0x21, 0x07, 22, 0xf7 }), "then the request");
                expect (sameBytes (sent.back(), { 0xf0, 0x42, 0x21, 0x0e, 1, 0xf7 }), "panel bank selected again");
            }
        }
    }
};

static Hawk800DeviceTests hawk800DeviceTests;
