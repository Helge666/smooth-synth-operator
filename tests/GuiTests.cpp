#include <sso_kiwi3p/sso_kiwi3p.h>

namespace
{
    struct DummyProcessor final : public juce::AudioProcessor
    {
        const juce::String getName() const override { return "test"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return true; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    std::vector<juce::TextButton*> findButtons (juce::Component& parent)
    {
        std::vector<juce::TextButton*> result;

        for (auto* child : parent.getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (child))
                result.push_back (b);

        return result;
    }
}

class GuiTests final : public juce::UnitTest
{
public:
    GuiTests() : juce::UnitTest ("GUI", "SSO") {}

    void runTest() override
    {
        sso::kiwi3p::Kiwi3PDevice device;
        DummyProcessor processor;
        juce::AudioProcessorValueTreeState state (processor, nullptr, "test", sso::SynthController::createParameterLayout (device));
        sso::SynthController controller (device, state);

        auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("sso-gui-test-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()));
        const juce::ScopeGuard cleanup { [&root] { root.deleteRecursively(); } };

        sso::PresetLibrary library (device, root);

        for (auto* bank : { "Kiwi Factory", "My Sounds" })
        {
            sso::Preset p;
            p.name = juce::String ("Sound in ") + bank;
            p.category = "Strings";
            p.patch = device.createInitPatch();
            library.add (p, bank);
        }

        beginTest ("Clicking every filter chip does not crash and filters");
        {
            sso::PresetBrowser browser (library, controller);
            browser.setSize (330, 900);

            // Click each chip by its label; the set of buttons may be rebuilt, so look them up each time.
            juce::StringArray labels;
            for (auto* b : findButtons (browser))
                labels.add (b->getButtonText());

            for (auto& label : labels)
            {
                if (label == "SAVE" || label.startsWith ("SAVE AS") || label.startsWith ("IMPORT"))
                    continue;

                for (auto* b : findButtons (browser))
                {
                    if (b->getButtonText() == label)
                    {
                        b->triggerClick();
                        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
                        break;
                    }
                }
            }

            // Bank filter actually applies
            for (auto* b : findButtons (browser))
                if (b->getButtonText() == "Kiwi Factory")
                    b->triggerClick();

            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);

            int toggled = 0;
            for (auto* b : findButtons (browser))
                if (b->getToggleState())
                    ++toggled;

            expect (toggled >= 1, "the selected chip is highlighted");
        }

        beginTest ("Dialogs close on Save and Cancel");
        {
            sso::PresetBrowser browser (library, controller);
            browser.setSize (330, 900);

            auto findDialog = []() -> juce::AlertWindow*
            {
                for (int i = 0; i < juce::Desktop::getInstance().getNumComponents(); ++i)
                    if (auto* w = dynamic_cast<juce::AlertWindow*> (juce::Desktop::getInstance().getComponent (i)))
                        if (w->isVisible())
                            return w;

                return nullptr;
            };

            auto click = [] (juce::AlertWindow& w, const juce::String& text)
            {
                for (auto* child : w.getChildren())
                    if (auto* b = dynamic_cast<juce::TextButton*> (child); b != nullptr && b->getButtonText() == text)
                        b->triggerClick();

                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            };

            for (auto* buttonText : { "Cancel", "Save" })
            {
                auto before = (int) library.getPresets().size();
                browser.saveAs();
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

                auto* dialog = findDialog();
                expect (dialog != nullptr, "save dialog shown");

                if (dialog == nullptr)
                    continue;

                click (*dialog, buttonText);
                expect (findDialog() == nullptr, juce::String ("dialog still open after ") + buttonText);
                expectEquals ((int) library.getPresets().size(), before + (juce::String (buttonText) == "Save" ? 1 : 0));
            }
        }
    }
};

static GuiTests guiTests;
