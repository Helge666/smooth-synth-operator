#pragma once

#include "PluginProcessor.h"
#include "Kiwi3PPanel.h"

/** Fixed-size design (browser + console) scaled to the window size. */
class Kiwi3PEditor final : public juce::AudioProcessorEditor
{
public:
    explicit Kiwi3PEditor (Kiwi3PProcessor&);
    ~Kiwi3PEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr int browserWidth = 330;
    static constexpr int consoleWidth = 1520;
    static constexpr int designHeight = 930;

private:
    struct Content final : public juce::Component
    {
        Content (Kiwi3PProcessor&);
        void paint (juce::Graphics&) override;
        void resized() override;

        sso::PresetBrowser browser;
        sso::HeaderBar header;
        Kiwi3PPanel panel;
        sso::InactiveOverlay inactiveOverlay;
        bool browserVisible = true;
    };

    int designWidth() const noexcept { return content.browserVisible ? browserWidth + consoleWidth : consoleWidth; }
    void setBrowserVisible (bool);
    void updateAspect();

    bool isStandalone() const noexcept;

    sso::LookAndFeel lookAndFeel;
    juce::SharedResourcePointer<sso::AppSettings> settings;
    Content content;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Kiwi3PEditor)
};
