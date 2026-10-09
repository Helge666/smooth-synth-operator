#include <sso_kiwi3p/sso_kiwi3p.h>

class GeneratorTests final : public juce::UnitTest
{
public:
    GeneratorTests() : juce::UnitTest ("Kiwi-3P generator", "SSO") {}

    void runTest() override
    {
        sso::kiwi3p::Kiwi3PDevice device;
        auto generator = sso::kiwi3p::createGenerator (device);
        juce::Random random (1234);
        auto& params = device.getParams();
        auto get = [&] (const sso::Patch& p, const char* id) { return device.getValue (p, device.indexOf (id)); };

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

                expectEquals ((int) p.data.size(), (int) sso::kiwi3p::Kiwi3PDevice::patchSize);
                expect (get (p, "vca_level") >= 70, what + " too quiet");
                expectEquals (get (p, "vca_mode"), 2, what + " VCA not on ENV-2");
                expect (get (p, "cutoff") + juce::jmax (0, get (p, "vcf_env") - 63) >= 35, what + " filter closed");
                expect (get (p, "env2_sustain") >= 20 || get (p, "env2_decay") >= 30, what + " amp envelope too short");

                auto patchName = device.getPatchName (p);
                expect (patchName.isNotEmpty() && patchName.length() <= sso::kiwi3p::Kiwi3PDevice::nameLength, what + " name " + patchName);

                // the unused bytes keep their value
                expectEquals ((int) p.data[0x18], 0x3f);

                if (cat == "Bass")      expect (get (p, "dco1_range") <= 1, what + " DCO-1 not 16'/8'");
                if (cat == "Pads")      expect (get (p, "env2_attack") >= 40, what + " pad without slow attack");
                if (cat == "Keys")      expect (get (p, "env2_attack") <= 2, what + " keys with slow attack");
                if (cat == "Bells")     expect (get (p, "env2_sustain") <= 30, what + " bells sustain");
                if (cat == "Strings")   expect (get (p, "dco1_wave") == 0 && get (p, "dco2_wave") == 0, what + " strings not saw");
                if (cat == "Brass")     expect (get (p, "env1_attack") >= 10, what + " brass without filter swell");
            }
        }

        beginTest ("Keep groups stay untouched");
        {
            auto current = generator->generate ("FX", device.createInitPatch(), {}, random);
            juce::StringArray keep { "ENV", "MOD" };

            for (int n = 0; n < 100; ++n)
            {
                auto p = generator->generate ("Bass", current, keep, random);

                for (int i = 0; i < (int) params.size(); ++i)
                    if (keep.contains (generator->getGroupOf (i)))
                        expectEquals (device.getValue (p, i), device.getValue (current, i), params[(size_t) i].id);
            }
        }

        beginTest ("Variation changes the sound moderately and respects keep");
        {
            auto current = generator->generate ("Poly", device.createInitPatch(), {}, random);
            int changedTotal = 0;

            for (int n = 0; n < 100; ++n)
            {
                auto p = generator->vary (current, 0.15f, { "VCF" }, random);
                expectEquals (get (p, "vca_mode"), get (current, "vca_mode"));
                expectEquals (get (p, "cutoff"), get (current, "cutoff"), "kept VCF changed");

                for (int i = 0; i < (int) params.size(); ++i)
                {
                    auto& spec = params[(size_t) i];
                    auto delta = std::abs (device.getValue (p, i) - device.getValue (current, i));

                    if (! spec.isDiscrete() && delta > 0)
                    {
                        ++changedTotal;
                        expect (delta <= 25 || spec.id == "vca_level" || spec.id == "cutoff",
                                spec.id + " jumped by " + juce::String (delta));
                    }
                }
            }

            expect (changedTotal > 100, "variation hardly changes anything");
        }
    }
};

static GeneratorTests generatorTests;
