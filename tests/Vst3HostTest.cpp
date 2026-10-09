#include <sso_kiwi3p/sso_kiwi3p.h>

/** Loads the built VST3 like a DAW does and checks that a project's sound is restored and
    sent to the synth: SSO_Tests --vst3-state "<port name part>" [path to .vst3]

    Uses a plain MidiConnection to read the synth's edit buffer (a SynthController would
    compete with the plugin for the synth). Restores the original edit buffer at the end.
    Close other SSO instances (e.g. the standalone app) first - otherwise the plugin is
    correctly inactive and sends nothing. */
namespace
{
    void pumpMs (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

    juce::String portId (const juce::Array<juce::MidiDeviceInfo>& devices, const juce::String& part)
    {
        for (auto& d : devices)
            if (d.name.contains (part))
                return d.identifier;

        return {};
    }

    struct SynthReader
    {
        SynthReader (const juce::String& portPart)
        {
            connection.onMessageReceived = [this] (const juce::MidiMessage& m)
            {
                if (auto p = device.parsePatchDump (settings, m); p.has_value() && p->slot < 0)
                {
                    const juce::ScopedLock sl (lock);
                    received = p->patch;
                }
            };

            connection.setOutput (portId (juce::MidiOutput::getAvailableDevices(), portPart));
            connection.setInput (portId (juce::MidiInput::getAvailableDevices(), portPart));
        }

        std::optional<sso::Patch> editBuffer()
        {
            {
                const juce::ScopedLock sl (lock);
                received.reset();
            }

            connection.send (device.createEditBufferRequest (settings));

            for (int i = 0; i < 40; ++i)
            {
                pumpMs (25);
                const juce::ScopedLock sl (lock);

                if (received.has_value())
                    return received;
            }

            return std::nullopt;
        }

        void send (const sso::Patch& p) { connection.send (device.createEditBufferDump (settings, p)); pumpMs (200); }

        int value (const sso::Patch& p, const char* id) const { return device.getValue (p, device.indexOf (id)); }

        sso::kiwi3p::Kiwi3PDevice device;
        sso::DeviceDefinition::MidiSettings settings;
        sso::MidiConnection connection;
        juce::CriticalSection lock;
        std::optional<sso::Patch> received;
    };

    void process (juce::AudioPluginInstance& plugin, int blocks)
    {
        juce::AudioBuffer<float> audio (2, 512);
        juce::MidiBuffer midi;

        for (int i = 0; i < blocks; ++i)
        {
            audio.clear();
            midi.clear();
            plugin.processBlock (audio, midi);
            pumpMs (10);
        }
    }

    // VST3 only transports numeric parameter ids, so look parameters up by their name
    // (as a DAW shows them), e.g. "VCF Cutoff".
    juce::AudioProcessorParameter* findParameter (juce::AudioPluginInstance& plugin, const juce::String& name)
    {
        for (auto* p : plugin.getParameters())
            if (p->getName (100) == name)
                return p;

        return nullptr;
    }

    std::unique_ptr<juce::AudioPluginInstance> loadPlugin (juce::VST3PluginFormat& format, const juce::PluginDescription& desc)
    {
        juce::String error;
        auto plugin = format.createInstanceFromDescription (desc, 48000.0, 512, error);

        if (plugin != nullptr)
        {
            plugin->enableAllBuses();
            plugin->prepareToPlay (48000.0, 512);
            pumpMs (300);
        }
        else
        {
            std::cout << "  could not load plugin: " << error << std::endl;
        }

        return plugin;
    }
}

int runVst3StateTest (const juce::StringArray& args)
{
    auto portPart = args[args.indexOf ("--vst3-state") + 1];
    auto pathArg = args[args.indexOf ("--vst3-state") + 2];
    auto vst3 = pathArg.endsWith (".vst3") ? juce::File (pathArg)
                                           : juce::File::getCurrentWorkingDirectory()
                                                 .getChildFile ("build/apps/SSO-Kiwi3P/SSO_Kiwi3P_artefacts/Debug/VST3/SSO Kiwi-3P.vst3");
    int failures = 0;
    auto check = [&failures] (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  ok    " : "  FAIL  ") << what << std::endl;
        if (! ok) ++failures;
    };

    SynthReader synth (portPart);
    auto original = synth.editBuffer();
    check (original.has_value(), "read the synth's edit buffer (" + portPart + ")");

    if (! original.has_value())
        return 1;

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile (descriptions, vst3.getFullPathName());
    check (! descriptions.isEmpty(), "found plugin in " + vst3.getFullPathName());

    if (descriptions.isEmpty())
        return 1;

    juce::MemoryBlock projectState;

    // --- "Project 1": edit the sound, save the project, close it
    {
        auto plugin = loadPlugin (format, *descriptions[0]);
        check (plugin != nullptr, "instance A loaded");

        if (plugin == nullptr)
            return 1;

        auto* cutoff = findParameter (*plugin, "VCF Cutoff");
        auto* reso = findParameter (*plugin, "VCF Resonance");
        check (cutoff != nullptr && reso != nullptr, "parameters found");

        if (cutoff == nullptr || reso == nullptr)
            return 1;

        cutoff->setValueNotifyingHost (23.0f / 127.0f);
        reso->setValueNotifyingHost (77.0f / 127.0f);
        process (*plugin, 30);
        pumpMs (300);

        auto afterEdit = synth.editBuffer();
        check (afterEdit.has_value() && synth.value (*afterEdit, "cutoff") == 23 && synth.value (*afterEdit, "resonance") == 77,
               "host parameter changes reach the synth (cutoff 23, resonance 77)"
                   + juce::String (afterEdit.has_value() ? "" : " - no answer"));

        plugin->getStateInformation (projectState);
        check (projectState.getSize() > 0, "project state saved (" + juce::String ((int) projectState.getSize()) + " bytes)");
        plugin->releaseResources();
    }

    pumpMs (300);

    // Something else is played on the synth in between
    synth.send (*original);
    auto between = synth.editBuffer();
    check (between.has_value() && synth.value (*between, "cutoff") != 23, "synth has a different sound before reopening");

    // --- "Project 1 reopened": a fresh instance restores the saved state
    {
        auto plugin = loadPlugin (format, *descriptions[0]);
        check (plugin != nullptr, "instance B loaded");

        if (plugin != nullptr)
        {
            plugin->setStateInformation (projectState.getData(), (int) projectState.getSize());
            process (*plugin, 30);
            pumpMs (500);

            auto restored = synth.editBuffer();
            check (restored.has_value() && synth.value (*restored, "cutoff") == 23 && synth.value (*restored, "resonance") == 77,
                   "reopened project sends its sound to the synth");

            auto* cutoff = findParameter (*plugin, "VCF Cutoff");
            check (cutoff != nullptr && juce::roundToInt (cutoff->getValue() * 127.0f) == 23, "restored parameter value in the plugin");
            plugin->releaseResources();
        }
    }

    pumpMs (300);

    // --- Two tracks with the plugin: only the first controls the synth
    {
        auto first = loadPlugin (format, *descriptions[0]);
        auto second = loadPlugin (format, *descriptions[0]);
        check (first != nullptr && second != nullptr, "two instances loaded");

        if (first != nullptr && second != nullptr)
        {
            synth.send (*original);
            auto before = synth.value (*synth.editBuffer(), "cutoff");
            pumpMs (500);   // the active instance also receives that answer and adopts it - let it settle

            findParameter (*second, "VCF Cutoff")->setValueNotifyingHost (5.0f / 127.0f);
            process (*second, 30);
            pumpMs (300);
            auto afterSecond = synth.editBuffer();
            check (afterSecond.has_value() && synth.value (*afterSecond, "cutoff") == before,
                   "second (inactive) instance sends nothing");

            auto* firstCutoff = findParameter (*first, "VCF Cutoff");
            firstCutoff->setValueNotifyingHost (99.0f / 127.0f);
            process (*first, 30);
            pumpMs (300);
            std::cout << "  (first instance cutoff parameter now " << juce::roundToInt (firstCutoff->getValue() * 127.0f) << ")" << std::endl;
            auto afterFirst = synth.editBuffer();
            check (afterFirst.has_value() && synth.value (*afterFirst, "cutoff") == 99,
                   "first (active) instance controls the synth (cutoff before " + juce::String (before) + ", after "
                       + (afterFirst.has_value() ? juce::String (synth.value (*afterFirst, "cutoff")) : juce::String ("no answer")) + ")");

            first->releaseResources();
            second->releaseResources();
        }
    }

    pumpMs (300);
    synth.send (*original);
    auto finalState = synth.editBuffer();
    check (finalState.has_value() && *finalState == *original, "original edit buffer restored");

    std::cout << (failures == 0 ? "VST3 STATE TEST PASSED" : "VST3 STATE TEST FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
