#include <sso_kiwi3p/sso_kiwi3p.h>
#include <sso_kiwisix/sso_kiwisix.h>
#include <sso_hawk800/sso_hawk800.h>

/*  sso-cli - library maintenance from the command line

    All commands take [--device kiwi3p|kiwisix|hawk800] (default kiwi3p).

    sso-cli import <file.syx> --bank <name> [--slots A-1:B-16] [--skip-generic]
                              [--author <name>] [--root <library dir>]
    sso-cli list [--bank <name>] [--root <library dir>]
    sso-cli generate <category|all> --count <n> --out <file.syx> [--seed <n>]
    sso-cli import-hawk-factory <folder with Bank1-4.syx> [--bank <name>] --device hawk800
*/

namespace
{
    juce::String option (const juce::StringArray& args, const char* name, const juce::String& fallback = {})
    {
        auto i = args.indexOf (name);
        return i >= 0 && i + 1 < args.size() ? args[i + 1] : fallback;
    }

    int slotFromName (const sso::DeviceDefinition& device, const juce::String& name)
    {
        for (int s = 0; s < device.getNumSlots(); ++s)
            if (device.getSlotName (s).equalsIgnoreCase (name.trim()))
                return s;

        return -1;
    }

    int usage()
    {
        std::cout << "usage:\n"
                     "  sso-cli import <file.syx> --bank <name> [--slots A-1:B-16] [--skip-generic] [--author <name>] [--root <dir>]\n"
                     "  sso-cli list [--bank <name>] [--root <dir>]\n"
                     "  sso-cli generate <category|all> --count <n> --out <file.syx> [--seed <n>]\n"
                     "  all commands: [--device kiwi3p|kiwisix|hawk800]\n";
        return 2;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::StringArray args (argv + 1, argc - 1);

    if (args.isEmpty())
        return usage();

    auto deviceName = option (args, "--device", "kiwi3p");
    std::unique_ptr<sso::DeviceDefinition> devicePtr;

    if (deviceName == "kiwisix")
        devicePtr = std::make_unique<sso::kiwisix::KiwiSixDevice>();
    else if (deviceName == "hawk800")
        devicePtr = std::make_unique<sso::hawk800::Hawk800Device>();
    else if (deviceName == "kiwi3p")
        devicePtr = std::make_unique<sso::kiwi3p::Kiwi3PDevice>();
    else
        return usage();

    auto& device = *devicePtr;
    auto rootArg = option (args, "--root");
    sso::PresetLibrary library (device, rootArg.isNotEmpty() ? juce::File (rootArg) : juce::File());

    if (args[0] == "list")
    {
        sso::PresetLibrary::Filter filter;
        filter.bank = option (args, "--bank");

        for (auto i : library.search (filter))
        {
            auto& p = library.getPresets()[(size_t) i];
            std::cout << p.bank << " | " << p.name << " | " << p.category << " | " << p.origin << "\n";
        }

        std::cout << library.getPresets().size() << " presets in " << library.getRoot().getFullPathName() << std::endl;
        return 0;
    }

    if (args[0] == "generate" && args.size() >= 2)
    {
        auto generator = deviceName == "kiwisix" ? sso::kiwisix::createGenerator (static_cast<const sso::kiwisix::KiwiSixDevice&> (device))
                       : deviceName == "hawk800" ? sso::hawk800::createGenerator (static_cast<const sso::hawk800::Hawk800Device&> (device))
                                                 : sso::kiwi3p::createGenerator (static_cast<const sso::kiwi3p::Kiwi3PDevice&> (device));
        auto categories = args[1] == "all" ? generator->getCategories() : juce::StringArray { args[1] };
        auto count = juce::jmax (1, option (args, "--count", "1").getIntValue());
        juce::Random random (option (args, "--seed", juce::String (juce::Time::currentTimeMillis())).getLargeIntValue());
        juce::File out (option (args, "--out"));

        if (out == juce::File() || ! generator->getCategories().contains (categories[0]))
            return usage();

        juce::MemoryOutputStream syx;

        for (auto& category : categories)
        {
            for (int i = 0; i < count; ++i)
            {
                auto patch = generator->generate (category, device.createInitPatch(), {}, random);
                auto m = device.createEditBufferDump ({}, patch);
                syx.write (m.getRawData(), (size_t) m.getRawDataSize());
                std::cout << category << ": " << device.getPatchName (patch) << "\n";
            }
        }

        out.replaceWithData (syx.getData(), syx.getDataSize());
        std::cout << "wrote " << out.getFullPathName() << std::endl;
        return 0;
    }

    if (args[0] == "import-hawk-factory" && args.size() >= 2 && deviceName == "hawk800")
    {
        auto result = sso::hawk800::importFactoryBanks (library, static_cast<const sso::hawk800::Hawk800Device&> (device),
                                                        juce::File (args[1]), option (args, "--bank", "HAWK Factory"));
        std::cout << "imported " << result.imported << " -> " << library.getRoot().getFullPathName() << std::endl;
        return result.imported > 0 ? 0 : 1;
    }

    if (args[0] == "import" && args.size() >= 2)
    {
        juce::File syx (args[1]);
        auto bank = option (args, "--bank");

        if (! syx.existsAsFile() || bank.isEmpty())
            return usage();

        auto patches = library.parseSyx (syx);
        int first = 0, last = device.getNumSlots() - 1;

        if (auto range = option (args, "--slots"); range.isNotEmpty())
        {
            first = slotFromName (device, range.upToFirstOccurrenceOf (":", false, false));
            last  = slotFromName (device, range.fromFirstOccurrenceOf (":", false, false));

            if (first < 0 || last < first)
                return usage();
        }

        // "Generic" = the sound that occurs most often in the file (the empty factory tone)
        sso::Patch generic;

        if (args.contains ("--skip-generic"))
        {
            int best = 0;

            for (auto& a : patches)
            {
                auto n = (int) std::count_if (patches.begin(), patches.end(), [&a] (auto& b) { return b.patch == a.patch; });

                if (n > best)
                {
                    best = n;
                    generic = a.patch;
                }
            }
        }

        auto result = library.importPatches (patches, bank, syx.getFileName(),
            [&] (const sso::DeviceDefinition::IncomingPatch& p)
            {
                if (p.slot >= 0 && (p.slot < first || p.slot > last))
                    return false;

                return ! (generic.isValid() && p.patch == generic);
            },
            option (args, "--author"));

        std::cout << "imported " << result.imported << ", duplicates " << result.duplicates
                  << ", skipped " << result.rejected << " -> " << library.getRoot().getChildFile (bank).getFullPathName() << std::endl;
        return 0;
    }

    return usage();
}
