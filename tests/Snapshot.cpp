#include "../apps/SSO-Kiwi3P/Source/PluginEditor.h"
#include "../apps/SSO-KiwiSix/Source/PluginEditor.h"

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
        auto savedScale = settings->get().getValue (scaleKey);
        auto savedBrowser = settings->get().getValue (browserKey);
        const juce::ScopeGuard restoreSettings { [&]
        {
            settings->get().setValue (scaleKey, savedScale);
            settings->get().setValue (browserKey, savedBrowser);
            settings->get().saveIfNeeded();
        } };
        settings->get().setValue (browserKey, ! args.contains ("--no-browser"));

        Processor processor;

        // --inactive: a second instance for the same synth shows the inactive overlay
        std::unique_ptr<Processor> second;
        if (args.contains ("--inactive"))
        {
            second = std::make_unique<Processor>();
            processor.controller.setInstanceDescription ("Cubase - track \"Pad\"");
        }

        std::unique_ptr<juce::AudioProcessorEditor> editor ((second != nullptr ? *second : processor).createEditor());

        auto cheeks = sso::theme::woodCheeks ? 2 * sso::ProgrammerEditor::cheekWidth : 0;
        auto designWidth = Editor::consoleWidth + cheeks + (args.contains ("--no-browser") ? 0 : Editor::browserWidth);
        editor->setSize (designWidth, Editor::designHeight);

        // let timers/async updates settle (LEDs, display)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);

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

/** Renders a plugin editor offscreen to a PNG - for reviewing the GUI without a host.
    Usage: SSO_Tests --snapshot out.png [--device kiwi3p|kiwisix] [--no-browser] [--inactive] [--scale 1.0] */
int runSnapshot (const juce::StringArray& args)
{
    auto deviceIndex = args.indexOf ("--device");
    auto device = deviceIndex >= 0 ? args[deviceIndex + 1] : juce::String ("kiwi3p");

    if (device == "kiwisix")
        return snapshot<KiwiSixProcessor, KiwiSixEditor> (args, "kiwisix");

    return snapshot<Kiwi3PProcessor, Kiwi3PEditor> (args, "kiwi3p");
}
