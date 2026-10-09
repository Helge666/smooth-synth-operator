#include <sso_kiwi3p/sso_kiwi3p.h>

/** Reproduces "plugin is inactive because another process owns the synth" through the
    plugin's real editor: SSO_Tests --vst3-takeover [path to .vst3]
    Logs the overlay/button state, clicks TAKE OVER and waits for the overlay to go away. */
namespace
{
    void pumpFor (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

    void collect (juce::Component& c, std::vector<juce::Component*>& out)
    {
        out.push_back (&c);

        for (auto* child : c.getChildren())
            collect (*child, out);
    }

    juce::TextButton* findButton (juce::Component& root, const juce::String& text)
    {
        std::vector<juce::Component*> all;
        collect (root, all);

        for (auto* c : all)
            if (auto* b = dynamic_cast<juce::TextButton*> (c); b != nullptr && b->getButtonText() == text)
                return b;

        return nullptr;
    }
}

int runVst3TakeOverTest (const juce::StringArray& args)
{
    auto pathArg = args[args.indexOf ("--vst3-takeover") + 1];
    auto vst3 = pathArg.endsWith (".vst3") ? juce::File (pathArg)
                                           : juce::File::getCurrentWorkingDirectory()
                                                 .getChildFile ("build/apps/SSO-Kiwi3P/SSO_Kiwi3P_artefacts/Release/VST3/SSO Kiwi-3P.vst3");
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile (descriptions, vst3.getFullPathName());

    if (descriptions.isEmpty())
    {
        std::cout << "plugin not found: " << vst3.getFullPathName() << std::endl;
        return 1;
    }

    juce::String error;
    auto plugin = format.createInstanceFromDescription (*descriptions[0], 48000.0, 512, error);

    if (plugin == nullptr)
    {
        std::cout << "could not load: " << error << std::endl;
        return 1;
    }

    plugin->prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor (plugin->createEditorIfNeeded());
    pumpFor (800);

    // Put the editor on screen like a host does, so mouse handling is real
    juce::DocumentWindow window ("SSO take-over test", juce::Colours::black, 0);
    window.setContentNonOwned (editor.get(), true);
    window.setVisible (true);
    pumpFor (800);

    auto* button = findButton (*editor, "TAKE OVER");
    std::cout << "TAKE OVER button: " << (button == nullptr ? "not found"
                                         : juce::String ("showing=") + (button->isShowing() ? "yes" : "no")
                                               + " enabled=" + (button->isEnabled() ? "yes" : "no")
                                               + " parentVisible=" + (button->getParentComponent()->isVisible() ? "yes" : "no"))
              << std::endl;

    if (button == nullptr || ! button->isShowing())
        return 1;

    // Is something else catching the mouse at the button's position?
    auto centre = button->getScreenBounds().getCentre();
    auto* hit = juce::Desktop::getInstance().findComponentAt (centre);
    std::cout << "component under the button centre: " << (hit == nullptr ? juce::String ("none")
                                                                             : juce::String (typeid (*hit).name()))
              << (hit == button ? " (the button)" : " (NOT the button)") << std::endl;

    button->triggerClick();

    for (int i = 0; i < 40 && button->getParentComponent()->isVisible(); ++i)
        pumpFor (100);

    auto taken = ! button->getParentComponent()->isVisible();
    std::cout << (taken ? "TAKE OVER WORKED" : "STILL INACTIVE after 4 s") << std::endl;

    window.clearContentComponent();
    editor.reset();
    plugin->releaseResources();
    return taken ? 0 : 1;
}
