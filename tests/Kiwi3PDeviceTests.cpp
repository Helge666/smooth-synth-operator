#include <sso_kiwi3p/sso_kiwi3p.h>

using sso::kiwi3p::Kiwi3PDevice;

/** Splits a .syx file into individual SysEx messages. */
static std::vector<juce::MidiMessage> readSyx (const juce::File& file)
{
    juce::MemoryBlock mb;
    file.loadFileAsData (mb);
    auto* d = static_cast<const uint8_t*> (mb.getData());
    std::vector<juce::MidiMessage> result;

    for (size_t i = 0; i < mb.getSize(); ++i)
    {
        if (d[i] != 0xf0)
            continue;

        auto end = i + 1;
        while (end < mb.getSize() && d[end] != 0xf7)
            ++end;

        result.push_back (juce::MidiMessage::createSysExMessage (d + i + 1, (int) (end - i - 1)));
        i = end;
    }

    return result;
}

class Kiwi3PDeviceTests final : public juce::UnitTest
{
public:
    Kiwi3PDeviceTests() : juce::UnitTest ("Kiwi-3P device definition", "SSO") {}

    void runTest() override
    {
        Kiwi3PDevice device;
        sso::DeviceDefinition::MidiSettings midi;
        auto& params = device.getParams();

        beginTest ("Parameter table is consistent");
        {
            juce::StringArray ids;

            for (auto& p : params)
            {
                expect (! ids.contains (p.id), "duplicate id " + p.id);
                ids.add (p.id);
                expect (juce::isPositiveAndBelow (p.offset, Kiwi3PDevice::patchSize), p.id);
                expect (p.defaultValue >= p.minValue && p.defaultValue <= p.maxValue, p.id);
                expect (p.storedValues.empty() || (int) p.storedValues.size() == p.numValues(), p.id);
                expect (p.ccValues.empty() || (int) p.ccValues.size() == p.numValues(), p.id);
                expect (p.offset >= Kiwi3PDevice::nameLength, p.id + " overlaps the name");
            }

            // No two parameters may claim the same bits
            for (size_t a = 0; a < params.size(); ++a)
                for (size_t b = a + 1; b < params.size(); ++b)
                    if (params[a].offset == params[b].offset)
                    {
                        auto maskA = ((1 << params[a].bits) - 1) << params[a].shift;
                        auto maskB = ((1 << params[b].bits) - 1) << params[b].shift;
                        expect ((maskA & maskB) == 0, params[a].id + " overlaps " + params[b].id);
                    }

            logMessage (juce::String ((int) params.size()) + " parameters");
        }

        beginTest ("Value <-> stored round trip for every value");
        {
            auto patch = device.createInitPatch();

            for (int i = 0; i < (int) params.size(); ++i)
                for (int v = params[(size_t) i].minValue; v <= params[(size_t) i].maxValue; ++v)
                {
                    device.setValue (patch, i, v);
                    expectEquals (device.getValue (patch, i), v, params[(size_t) i].id);
                }
        }

        beginTest ("Backup of the real synth: decode + re-encode reproduces every byte");

        if (auto file = juce::File (SSO_LOCAL_TONE_BACKUP); ! file.existsAsFile())
        {
            logMessage ("skipped - no local backup at " + file.getFullPathName());
        }
        else
        {
            auto messages = readSyx (file);
            expectEquals ((int) messages.size(), Kiwi3PDevice::numSlots, file.getFullPathName());

            int slot = 0, lossy = 0;

            for (auto& m : messages)
            {
                auto incoming = device.parsePatchDump (midi, m);
                expect (incoming.has_value(), "slot " + juce::String (slot) + " not parsed");

                if (! incoming.has_value())
                    continue;

                expectEquals (incoming->slot, slot);

                // Rebuild from an init patch that only shares the raw "unknown" bytes
                auto rebuilt = incoming->patch;
                for (int i = 0; i < (int) params.size(); ++i)
                    device.setValue (rebuilt, i, params[(size_t) i].defaultValue);
                for (int i = 0; i < (int) params.size(); ++i)
                    device.setValue (rebuilt, i, device.getValue (incoming->patch, i));

                if (rebuilt.data != incoming->patch.data)
                {
                    ++lossy;
                    for (size_t b = 0; b < rebuilt.data.size(); ++b)
                        if (rebuilt.data[b] != incoming->patch.data[b])
                            logMessage ("slot " + device.getSlotName (slot) + " byte 0x" + juce::String::toHexString ((int) b)
                                        + ": " + juce::String (incoming->patch.data[b]) + " -> " + juce::String (rebuilt.data[b]));
                }

                // Re-encoding as a slot dump yields the original message
                auto dump = device.createSlotDump (midi, incoming->patch, slot);
                expect (dump.getRawDataSize() == m.getRawDataSize()
                         && std::memcmp (dump.getRawData(), m.getRawData(), (size_t) m.getRawDataSize()) == 0,
                        "dump differs for slot " + juce::String (slot));
                ++slot;
            }

            expectEquals (lossy, 0, "patches that change when round-tripped through parameters");

            auto first = device.parsePatchDump (midi, messages.front());
            expectEquals (device.getPatchName (first->patch), juce::String ("Low Strings"));
        }

        beginTest ("Messages");
        {
            auto req = device.createEditBufferRequest (midi);
            const uint8_t expectedReq[] = { 0xf0, 0x00, 0x21, 0x16, 0x60, 0x01, 0x00, 0x03, 0xf7 };
            expect (req.getRawDataSize() == 9 && std::memcmp (req.getRawData(), expectedReq, 9) == 0);

            auto valueOf = [&] (int i) { return params[(size_t) i].defaultValue; };
            auto cutoff = device.createParamMessages (midi, device.indexOf ("cutoff"), valueOf);
            expect (cutoff.size() == 1 && cutoff[0].isControllerOfType (41) && cutoff[0].getControllerValue() == 127);

            // Mod wheel routing has no CC -> single-parameter SysEx with the whole byte
            auto mwValues = [&] (int i)
            {
                if (params[(size_t) i].id == "mw_dco_lfo") return 3;
                if (params[(size_t) i].id == "mw_mix") return 1;
                return params[(size_t) i].defaultValue;
            };
            auto mw = device.createParamMessages (midi, device.indexOf ("mw_dco_lfo"), mwValues);
            const uint8_t expectedMw[] = { 0xf0, 0x00, 0x21, 0x16, 0x60, 0x01, 0x00, 0x0e, 0x45, 0x4c, 0xf7 };
            expect (mw.size() == 1 && mw[0].getRawDataSize() == 11 && std::memcmp (mw[0].getRawData(), expectedMw, 11) == 0);

            // VCA mode uses inverted flag order: Gate -> stored bit0
            auto vca = device.indexOf ("vca_mode");
            auto patch = device.createInitPatch();
            device.setValue (patch, vca, 0);
            expectEquals ((int) patch.data[0x3e] & 3, 1);
            patch.data[0x3e] = 3;   // gate + env2 flag: gate wins
            expectEquals (device.getValue (patch, vca), 0);
        }

        beginTest ("Names");
        {
            auto patch = device.createInitPatch();
            device.setPatchName (patch, "A very long name that does not fit");
            expectEquals (device.getPatchName (patch), juce::String ("A very long name tha"));
            device.setPatchName (patch, juce::String::fromUTF8 ("Bässe"));
            expectEquals (device.getPatchName (patch), juce::String ("B?sse"));
            expectEquals (device.getSlotName (0), juce::String ("A-1"));
            expectEquals (device.getSlotName (255), juce::String ("P-16"));
        }
    }
};

static Kiwi3PDeviceTests kiwi3pDeviceTests;
