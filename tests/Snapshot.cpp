#include "../apps/SSO-Kiwi3P/Source/PluginEditor.h"
#include "../apps/SSO-KiwiSix/Source/PluginEditor.h"
#include "../apps/SSO-Hawk800/Source/PluginEditor.h"

namespace
{
    template <typename Processor, typename Editor>
    int snapshot (const juce::StringArray& args, const juce::String& deviceId)
    {
        auto out = juce::File::getCurrentWorkingDirectory().getChildFile (args[args.indexOf ("--snapshot") + 1]);
        auto scaleIndex = args.indexOf ("--scale");
        auto scale = scaleIndex >= 0 ? args[scaleIndex + 1].getFloatValue() : 1.0f;

        // The editor remembers its size and browser state; don't let the snapshot change the user's settings.
        juce::SharedResourcePointer<sso::AppSettings> settings;
        auto scaleKey = deviceId + ".scale", browserKey = deviceId + ".browserVisible";
        auto& props = settings->get();
        std::map<juce::String, std::optional<juce::String>> saved;

        for (auto key : { scaleKey, browserKey, deviceId + ".memoryOpen", deviceId + ".modPage", deviceId + ".aiControl" })
            saved[key] = props.containsKey (key) ? std::optional<juce::String> (props.getValue (key)) : std::nullopt;

        const juce::ScopeGuard restoreSettings { [&]
        {
            for (auto& [key, value] : saved)
            {
                if (value.has_value())
                    props.setValue (key, *value);
                else
                    props.removeValue (key);
            }

            props.saveIfNeeded();
        } };
        props.setValue (browserKey, ! args.contains ("--no-browser"));

        Processor processor;

        // --inactive: a second instance for the same synth shows the inactive overlay
        std::unique_ptr<Processor> second;
        if (args.contains ("--inactive"))
        {
            second = std::make_unique<Processor>();
            processor.controller.setInstanceDescription ("Cubase - track \"Pad\"");
        }

        if (args.contains ("--midi"))         // just the MIDI call-out; --ai-on: with AI control switched on
        {
            if (args.contains ("--ai-on"))
            {
                processor.aiControl.setEnabled (true);
                processor.controller.startUsing();
            }

            juce::MessageManager::getInstance()->runDispatchLoopUntil (500);
            sso::LookAndFeel lf;
            sso::MidiSettingsPanel panel (processor.controller, &processor.aiControl);
            panel.setLookAndFeel (&lf);
            auto image = juce::Image (juce::Image::ARGB, panel.getWidth(), panel.getHeight(), true);
            {
                juce::Graphics g (image);
                g.fillAll (sso::theme::panel);
                panel.paintEntireComponent (g, true);
            }
            out.deleteFile();
            juce::FileOutputStream stream (out);
            juce::PNGImageFormat().writeImageToStream (image, stream);
            panel.setLookAndFeel (nullptr);
            processor.aiControl.setEnabled (false);
            std::cout << "wrote " << out.getFullPathName() << std::endl;
            return 0;
        }

        if (args.contains ("--globals"))      // just the globals call-out
        {
            sso::LookAndFeel lf;
            sso::GlobalsPanel panel (processor.controller);
            panel.setLookAndFeel (&lf);
            auto image = juce::Image (juce::Image::ARGB, panel.getWidth(), panel.getHeight(), true);
            {
                juce::Graphics g (image);
                g.fillAll (sso::theme::panel);
                panel.paintEntireComponent (g, true);
            }
            out.deleteFile();
            juce::FileOutputStream stream (out);
            juce::PNGImageFormat().writeImageToStream (image, stream);
            panel.setLookAndFeel (nullptr);
            std::cout << "wrote " << out.getFullPathName() << std::endl;
            return 0;
        }

        // --tuning: TUNE pressed - on a dummy port, nothing reaches the real synth
        if (args.contains ("--tuning"))
        {
            processor.controller.getConnection().setInput ({});
            processor.controller.getConnection().setOutput ("SSO-TEST-PORT-snapshot");
            processor.controller.startUsing();
            processor.controller.setTuning (true);
        }

        std::unique_ptr<juce::AudioProcessorEditor> editor ((second != nullptr ? *second : processor).createEditor());

        auto cheeks = sso::theme::woodCheeks ? 2 * sso::ProgrammerEditor::cheekWidth : 0;
        auto designWidth = Editor::consoleWidth + cheeks + (args.contains ("--no-browser") ? 0 : Editor::browserWidth);
        editor->setSize (designWidth, Editor::designHeight);

        // let timers/async updates settle (LEDs, display)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (args.contains ("--tuning") ? 2500 : 300);

        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
        out.deleteFile();
        juce::FileOutputStream stream (out);
        juce::PNGImageFormat png;

        if (! stream.openedOk() || ! png.writeImageToStream (image, stream))
            return 1;

        std::cout << "wrote " << out.getFullPathName() << " (" << image.getWidth() << "x" << image.getHeight() << ")" << std::endl;
        return 0;
    }
}

/** Opens the KiwiSix editor in a window and folds/unfolds the synth memory with its SHOW/HIDE
    button, checking that the browser re-lays out. Usage: SSO_Tests --click-memory */
int runClickMemoryTest()
{
    KiwiSixProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->addToDesktop (juce::ComponentPeer::windowHasTitleBar);
    editor->setVisible (true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);

    sso::SynthMemoryPanel* memory = nullptr;
    std::function<void (juce::Component&)> find = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (auto* m = dynamic_cast<sso::SynthMemoryPanel*> (child))
                memory = m;
            else
                find (*child);
        }
    };
    find (*editor);

    juce::TextButton* fold = nullptr;
    for (auto* child : memory != nullptr ? memory->getChildren() : juce::Array<juce::Component*>())
        if (auto* b = dynamic_cast<juce::TextButton*> (child); b != nullptr && (b->getButtonText() == "SHOW" || b->getButtonText() == "HIDE"))
            fold = b;

    if (fold == nullptr)
        return 1;

    auto wasOpen = memory->isOpen();
    auto ok = true;

    for (int i = 0; i < 2; ++i)
    {
        auto before = memory->isOpen();
        fold->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        std::cout << "open " << memory->isOpen() << ", height " << memory->getHeight() << ", button " << fold->getButtonText() << std::endl;
        ok = ok && memory->isOpen() != before && memory->getHeight() == memory->getPreferredHeight();
    }

    memory->setOpen (wasOpen);
    std::cout << (ok ? "fold OK" : "fold FAILED") << std::endl;
    return ok ? 0 : 1;
}

/** Renders a plugin editor offscreen to a PNG - for reviewing the GUI without a host.
    Usage: SSO_Tests --snapshot out.png [--device kiwi3p|kiwisix|hawk800] [--no-browser] [--inactive] [--tuning] [--scale 1.0] */
int runSnapshot (const juce::StringArray& args)
{
    auto deviceIndex = args.indexOf ("--device");
    auto device = deviceIndex >= 0 ? args[deviceIndex + 1] : juce::String ("kiwi3p");

    if (device == "kiwisix")
        return snapshot<KiwiSixProcessor, KiwiSixEditor> (args, "kiwisix");

    if (device == "hawk800")
        return snapshot<Hawk800Processor, Hawk800Editor> (args, "hawk800");

    return snapshot<Kiwi3PProcessor, Kiwi3PEditor> (args, "kiwi3p");
}
