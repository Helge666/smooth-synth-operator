#include <sso_kiwisix/sso_kiwisix.h>

using sso::kiwisix::KiwiSixDevice;

namespace
{
    /** Splits a .syx file into individual SysEx messages. */
    std::vector<juce::MidiMessage> readSyxFile (const juce::File& file)
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

    bool sameBytes (const juce::MidiMessage& m, std::initializer_list<uint8_t> bytes)
    {
        std::vector<uint8_t> v (bytes);
        return m.getRawDataSize() == (int) v.size() && std::memcmp (m.getRawData(), v.data(), v.size()) == 0;
    }
}

class KiwiSixDeviceTests final : public juce::UnitTest
{
public:
    KiwiSixDeviceTests() : juce::UnitTest ("KiwiSix device definition", "SSO") {}

    void runTest() override
    {
        KiwiSixDevice device;
        sso::DeviceDefinition::MidiSettings midi;
        auto& params = device.getParams();

        beginTest ("Parameter table is consistent");
        {
            juce::StringArray ids;
            std::map<int, int> usedBits;    // offset -> mask

            for (auto& p : params)
            {
                expect (! ids.contains (p.id), "duplicate id " + p.id);
                ids.add (p.id);
                expect (p.offset >= KiwiSixDevice::nameLength && p.offset < KiwiSixDevice::patchSize, p.id);
                expect (p.defaultValue >= p.minValue && p.defaultValue <= p.maxValue, p.id);
                expect (p.storedValues.empty() || (int) p.storedValues.size() == p.numValues(), p.id);
                expect (p.ccValues.empty() || (int) p.ccValues.size() == p.numValues(), p.id);

                auto claim = [&] (int offset, int mask)
                {
                    expect ((usedBits[offset] & mask) == 0, p.id + " overlaps another parameter at 0x" + juce::String::toHexString (offset));
                    usedBits[offset] |= mask;
                };

                claim (p.offset, ((1 << p.bits) - 1) << p.shift);

                if (p.isWide())
                    claim (p.loOffset, 0x7f);
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

        beginTest ("12-bit layout matches the measured bytes");
        {
            auto patch = device.createInitPatch();
            device.setValue (patch, device.indexOf ("cutoff"), (31 << 7) | 96);     // CC 41 = 127 stored hi 31, lo 96
            expectEquals ((int) patch.data[0x1c], 31);
            expectEquals ((int) patch.data[0x1d], 96);

            device.setValue (patch, device.indexOf ("mg_vcf"), 1);
            device.setValue (patch, device.indexOf ("mg_vco"), 0);
            device.setValue (patch, device.indexOf ("mg_wave"), 2);                 // saw
            expectEquals ((int) patch.data[0x36], 0x02 | (2 << 3));

            device.setValue (patch, device.indexOf ("fx_type"), 3);                 // ensemble
            expectEquals ((int) patch.data[0x39], 2);
            device.setValue (patch, device.indexOf ("fx_type"), 1);                 // chorus
            expectEquals ((int) patch.data[0x39], 4);
            expectEquals ((int) patch.data[0x42], 7, "init patch as the firmware stores it");
        }

        auto checkFile = [&] (const char* path, const char* what, bool expectSlots)
        {
            beginTest (juce::String (what) + ": decode + re-encode reproduces every byte");
            auto file = juce::File (path);

            if (! file.existsAsFile())
            {
                logMessage ("skipped - not found: " + file.getFullPathName());
                return;
            }

            auto messages = readSyxFile (file);
            expectEquals ((int) messages.size(), KiwiSixDevice::numSlots, file.getFullPathName());
            int slot = 0, lossy = 0;

            for (auto& m : messages)
            {
                auto incoming = device.parsePatchDump (midi, m);
                expect (incoming.has_value(), "slot " + juce::String (slot) + " not parsed");

                if (! incoming.has_value())
                    continue;

                if (expectSlots)
                    expectEquals (incoming->slot, slot);

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
                            logMessage (device.getSlotName (slot) + " byte 0x" + juce::String::toHexString ((int) b)
                                        + ": " + juce::String (incoming->patch.data[b]) + " -> " + juce::String (rebuilt.data[b]));
                }

                auto dump = device.createSlotDump (midi, incoming->patch, incoming->slot);
                expect (dump.getRawDataSize() == m.getRawDataSize()
                         && std::memcmp (dump.getRawData(), m.getRawData(), (size_t) m.getRawDataSize()) == 0,
                        "dump differs for slot " + juce::String (slot));
                ++slot;
            }

            expectEquals (lossy, 0, "patches that change when round-tripped through parameters");
            expectEquals (device.getPatchName (device.parsePatchDump (midi, messages.front())->patch), juce::String ("Brass"));
        };

        checkFile (SSO_LOCAL_KIWISIX_BACKUP, "Backup of the real synth", true);
        checkFile (SSO_LOCAL_KIWISIX_FACTORY, "KiwiTechnics tone set factory3", true);

        beginTest ("Messages");
        {
            expect (sameBytes (device.createEditBufferRequest (midi), { 0xf0, 0x00, 0x21, 0x16, 0x60, 0x02, 0x00, 0x03, 0xf7 }));
            expect (sameBytes (device.createSlotRequest (midi, 511), { 0xf0, 0x00, 0x21, 0x16, 0x60, 0x02, 0x00, 0x05, 0x03, 0x7f, 0xf7 }));

            auto values = [&] (std::map<juce::String, int> overrides)
            {
                return [&params, overrides] (int i)
                {
                    auto it = overrides.find (params[(size_t) i].id);
                    return it != overrides.end() ? it->second : params[(size_t) i].defaultValue;
                };
            };

            // 12 bit -> SysEx 0E offset hi lo (measured: exact, unlike CC)
            auto cutoff = device.createParamMessages (midi, device.indexOf ("cutoff"), values ({ { "cutoff", (16 << 7) | 85 } }));
            expect (cutoff.size() == 1 && sameBytes (cutoff[0], { 0xf0, 0x00, 0x21, 0x16, 0x60, 0x02, 0x00, 0x0e, 0x1c, 0x10, 0x55, 0xf7 }));

            // switches -> CC in the middle of the decoded range
            auto wave = device.createParamMessages (midi, device.indexOf ("vco_wave"), values ({ { "vco_wave", 2 } }));
            expect (wave.size() == 1 && wave[0].isControllerOfType (9) && wave[0].getControllerValue() == 96);
            auto fx = device.createParamMessages (midi, device.indexOf ("fx_type"), values ({ { "fx_type", 2 } }));
            expect (fx.size() == 1 && fx[0].isControllerOfType (118) && fx[0].getControllerValue() == 48);   // phase
            auto gate = device.createParamMessages (midi, device.indexOf ("vca_mode"), values ({ { "vca_mode", 1 } }));
            expect (gate.size() == 1 && gate[0].isControllerOfType (61) && gate[0].getControllerValue() < 64);

            // MG destinations share a byte with the wave: SysEx with the whole byte
            auto dest = device.createParamMessages (midi, device.indexOf ("mg_vca"),
                                                    values ({ { "mg_vco", 1 }, { "mg_vca", 1 }, { "mg_wave", 1 } }));
            expect (dest.size() == 1 && sameBytes (dest[0], { 0xf0, 0x00, 0x21, 0x16, 0x60, 0x02, 0x00, 0x0e, 0x36, 0x00, 0x0d, 0xf7 }));

            // arp range "off" has no CC
            auto arpOff = device.createParamMessages (midi, device.indexOf ("arp_range"), values ({ { "arp_range", 0 } }));
            expect (arpOff.size() == 1 && arpOff[0].isSysEx());
        }

        beginTest ("Names, slots and the write normalisation");
        {
            expectEquals (device.getSlotName (0), juce::String ("A1-1"));
            expectEquals (device.getSlotName (9), juce::String ("A2-2"));
            expectEquals (device.getSlotName (511), juce::String ("H8-8"));
            auto layout = device.getSlotLayout();
            expectEquals (layout.banks * layout.groupsPerBank * layout.slotsPerGroup, KiwiSixDevice::numSlots);

            auto patch = device.createInitPatch();
            patch.data[0x42] = 0;
            expectEquals ((int) device.asStored (patch).data[0x42], 7);
            patch.data[0x42] = 3;
            expectEquals ((int) device.asStored (patch).data[0x42], 3);
        }
    }
};

static KiwiSixDeviceTests kiwiSixDeviceTests;

//==============================================================================
class KiwiSixGeneratorTests final : public juce::UnitTest
{
public:
    KiwiSixGeneratorTests() : juce::UnitTest ("KiwiSix generator", "SSO") {}

    void runTest() override
    {
        KiwiSixDevice device;
        auto generator = sso::kiwisix::createGenerator (device);
        juce::Random random (4321);
        auto& params = device.getParams();
        auto get = [&] (const sso::Patch& p, const char* id) { return device.getValue (p, device.indexOf (id)); };
        constexpr int step = 32;

        beginTest ("Every parameter belongs to a keep group");
        for (int i = 0; i < (int) params.size(); ++i)
            expect (generator->getGroupOf (i).isNotEmpty(), params[(size_t) i].id + " has no group");

        expectEquals (generator->getCategories().size(), 8);

        beginTest ("Generated sounds are playable and fit their category");
        for (auto& cat : generator->getCategories())
        {
            for (int n = 0; n < 300; ++n)
            {
                auto p = generator->generate (cat, device.createInitPatch(), {}, random);
                auto what = cat + " #" + juce::String (n);

                expectEquals ((int) p.data.size(), KiwiSixDevice::patchSize);
                expect (get (p, "vca_level") >= 1500 && get (p, "vca_level") <= 2600, what + " level out of the factory range");
                expect (get (p, "cutoff") + juce::jmax (0, get (p, "eg_int") - 2048) >= 35 * step, what + " filter closed");
                expect (get (p, "vca_mode") == 1 || get (p, "eg_sustain") >= 20 * step || get (p, "eg_decay") >= 55 * step,
                        what + " envelope too short");
                expectEquals ((int) p.data[0x42], 7, what + " not normalised");

                auto patchName = device.getPatchName (p);
                expect (patchName.isNotEmpty() && patchName.length() <= KiwiSixDevice::nameLength, what + " name " + patchName);

                if (cat == "Bass")      expect (get (p, "vco_octave") <= 1 && get (p, "eg_decay") >= 57 * step, what + " not a full bass");
                if (cat == "Pads")      expect (get (p, "eg_attack") >= 40 * step, what + " pad without slow attack");
                if (cat == "Keys")      expect (get (p, "eg_attack") <= 2 * step, what + " keys with slow attack");
                if (cat == "Bells")     expect (get (p, "eg_sustain") <= 20 * step, what + " bells sustain");
                if (cat == "Brass")     expect (get (p, "vco_wave") == 0, what + " brass not saw");
            }
        }

        beginTest ("Keep groups stay untouched");
        {
            auto current = generator->generate ("FX", device.createInitPatch(), {}, random);
            juce::StringArray keep { "EG", "MG" };

            for (int n = 0; n < 100; ++n)
            {
                auto p = generator->generate ("Bass", current, keep, random);

                for (int i = 0; i < (int) params.size(); ++i)
                    if (keep.contains (generator->getGroupOf (i)))
                        expectEquals (device.getValue (p, i), device.getValue (current, i), params[(size_t) i].id);
            }
        }

        beginTest ("Variation changes the sound moderately");
        {
            auto current = generator->generate ("Poly", device.createInitPatch(), {}, random);
            int changed = 0;

            for (int n = 0; n < 50; ++n)
            {
                auto p = generator->vary (current, 0.15f, { "VCF" }, random);
                expectEquals (get (p, "cutoff"), get (current, "cutoff"), "kept VCF changed");

                for (int i = 0; i < (int) params.size(); ++i)
                    if (! params[(size_t) i].isDiscrete() && device.getValue (p, i) != device.getValue (current, i))
                        ++changed;
            }

            expect (changed > 50, "variation hardly changes anything");
        }
    }
};

static KiwiSixGeneratorTests kiwiSixGeneratorTests;
