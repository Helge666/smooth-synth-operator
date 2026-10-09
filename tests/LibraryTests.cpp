#include <sso_kiwi3p/sso_kiwi3p.h>

class LibraryTests final : public juce::UnitTest
{
public:
    LibraryTests() : juce::UnitTest ("Preset library", "SSO") {}

    void runTest() override
    {
        sso::kiwi3p::Kiwi3PDevice device;
        auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("sso-library-test-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()));
        const juce::ScopeGuard cleanup { [&root] { root.deleteRecursively(); } };

        beginTest ("Preset file round trip");
        {
            sso::Preset p;
            p.name = "Fat Bass";
            p.category = "Bass";
            p.tags = { "warm", "mono" };
            p.author = "Test";
            p.comment = juce::String::fromUTF8 ("Umlaute: äöü");
            p.favorite = true;
            p.rating = 4;
            p.patch = device.createInitPatch();
            device.setValue (p.patch, device.indexOf ("cutoff"), 42);

            sso::PresetLibrary library (device, root);
            auto file = library.add (p, "My Sounds");
            expect (file.existsAsFile());
            expect (file.loadFileAsString().contains ("\"cutoff\": \"42\""), "readable params block");

            auto loaded = sso::Preset::load (file);
            expect (loaded.has_value());
            expectEquals (loaded->name, p.name);
            expectEquals (loaded->comment, p.comment);
            expect (loaded->tags == p.tags);
            expect (loaded->favorite && loaded->rating == 4);
            expect (loaded->patch == p.patch);
            expectEquals (loaded->deviceId, device.getDeviceId());

            // a second preset with the same name gets its own file
            auto file2 = library.add (p, "My Sounds");
            expect (file2 != file && file2.existsAsFile());
            expectEquals ((int) library.getPresets().size(), 2);
            expectEquals (library.getPresets()[0].bank, juce::String ("My Sounds"));

            auto copy = library.getPresets()[0];
            copy.category = "Lead";
            expect (library.update (copy));
            expect (library.remove (library.getPresets()[1]));
            expectEquals ((int) library.getPresets().size(), 1);
            expectEquals (library.getPresets()[0].category, juce::String ("Lead"));
        }

        beginTest ("Search and sort");
        {
            sso::PresetLibrary library (device, root);
            auto add = [&] (const char* presetName, const char* presetCategory, int slot, bool fav)
            {
                sso::Preset p;
                p.name = presetName; p.category = presetCategory; p.sortIndex = slot; p.favorite = fav;
                p.patch = device.createInitPatch();
                device.setPatchName (p.patch, presetName);
                library.add (p, "Search");
            };

            add ("Zeta Pad", "Pads", -1, false);
            add ("Alpha Strings", "Strings", 5, true);
            add ("Beta Strings", "Strings", 1, false);

            sso::PresetLibrary::Filter f;
            f.bank = "Search";
            auto all = library.search (f);
            expectEquals ((int) all.size(), 3);
            expectEquals (library.getPresets()[(size_t) all[0]].name, juce::String ("Beta Strings"));   // slot order first
            expectEquals (library.getPresets()[(size_t) all[2]].name, juce::String ("Zeta Pad"));       // no slot -> last

            f.text = "strings alpha";
            expectEquals ((int) library.search (f).size(), 1);
            f.text = {};
            f.category = "Strings";
            expectEquals ((int) library.search (f).size(), 2);
            f.favoritesOnly = true;
            expectEquals ((int) library.search (f).size(), 1);
            expect (library.getBanks().contains ("Search"));
        }

        beginTest ("Category guesses");
        {
            expectEquals (sso::Categories::guessFromName ("Electric Piano 2"), juce::String ("Keys"));
            expectEquals (sso::Categories::guessFromName ("Low Strings"), juce::String ("Strings"));
            expectEquals (sso::Categories::guessFromName ("Synth Brass 1"), juce::String ("Brass"));
            expectEquals (sso::Categories::guessFromName ("Sync Sweep"), juce::String ("FX"));
            expectEquals (sso::Categories::guessFromName ("Vibraphone"), juce::String ("Bells"));
            expectEquals (sso::Categories::guessFromName ("Something"), juce::String ("Other"));
        }

        beginTest ("Import of the real backup");

        if (auto syx = juce::File (SSO_LOCAL_TONE_BACKUP); ! syx.existsAsFile())
        {
            logMessage ("skipped - no local backup");
        }
        else
        {
            sso::PresetLibrary library (device, root);
            auto onlyBankA = [] (const sso::DeviceDefinition::IncomingPatch& p) { return p.slot < 16; };
            auto r = library.importSyx (syx, "Factory", onlyBankA, "KiwiTechnics");
            expectEquals (r.imported, 16);
            expectEquals (r.rejected, 240);

            auto again = library.importSyx (syx, "Factory", onlyBankA);
            expectEquals (again.imported, 0);
            expectEquals (again.duplicates, 16);

            sso::PresetLibrary::Filter f;
            f.bank = "Factory";
            auto found = library.search (f);
            auto& first = library.getPresets()[(size_t) found[0]];
            expectEquals (first.name, juce::String ("Low Strings"));
            expectEquals (first.category, juce::String ("Strings"));
            expectEquals (first.author, juce::String ("KiwiTechnics"));
            expect (first.origin.startsWith ("Synth A-1"));
        }
    }
};

static LibraryTests libraryTests;
