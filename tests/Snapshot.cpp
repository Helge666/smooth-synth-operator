#include "../apps/SSO-Kiwi3P/Source/PluginEditor.h"

/** Renders the plugin editor offscreen to a PNG - for reviewing the GUI without a host.
    Usage: SSO_Tests --snapshot out.png [--no-browser] [--scale 1.0] */
int runSnapshot (const juce::StringArray& args)
{
    auto out = juce::File::getCurrentWorkingDirectory().getChildFile (args[args.indexOf ("--snapshot") + 1]);
    auto scaleIndex = args.indexOf ("--scale");
    auto scale = scaleIndex >= 0 ? args[scaleIndex + 1].getFloatValue() : 1.0f;

    // The editor remembers its size and browser state; don't let the snapshot change the user's settings.
    juce::SharedResourcePointer<sso::AppSettings> settings;
    auto savedScale = settings->get().getValue ("kiwi3p.scale");
    auto savedBrowser = settings->get().getValue ("kiwi3p.browserVisible");
    const juce::ScopeGuard restoreSettings { [&]
    {
        settings->get().setValue ("kiwi3p.scale", savedScale);
        settings->get().setValue ("kiwi3p.browserVisible", savedBrowser);
        settings->get().saveIfNeeded();
    } };
    settings->get().setValue ("kiwi3p.browserVisible", ! args.contains ("--no-browser"));

    Kiwi3PProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    auto designWidth = Kiwi3PEditor::consoleWidth + (args.contains ("--no-browser") ? 0 : Kiwi3PEditor::browserWidth);
    editor->setSize (designWidth, Kiwi3PEditor::designHeight);

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
