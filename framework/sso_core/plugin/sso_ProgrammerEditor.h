#pragma once

namespace sso
{

/** The editor shared by all programmers: browser, header and the device's panel in a fixed
    design size, scaled to the window. The browser can be hidden.
*/
class ProgrammerEditor : public juce::AudioProcessorEditor
{
public:
    struct Layout
    {
        juce::String productLine;           // under the SSO logo, e.g. "Kiwi-3P Programmer"
        juce::String version;               // the app's version, e.g. "1.0.3" (SSO_APP_VERSION)
        int consoleWidth = 1520;            // header + panel, without the browser
        int designHeight = 930;
        int panelGap = 14;                  // between header and panel
    };

    ProgrammerEditor (ProgrammerProcessor&, std::unique_ptr<juce::Component> panel, Layout);
    ~ProgrammerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr int browserWidth = 330;
    static constexpr int cheekWidth = 16;   // wooden end cheeks (theme::woodCheeks)

private:
    struct Content final : public juce::Component
    {
        Content (ProgrammerProcessor&, std::unique_ptr<juce::Component> panel, const Layout&);
        void paint (juce::Graphics&) override;
        void resized() override;

        juce::Rectangle<int> consoleArea() const;

        const Layout layout;
        PresetBrowser browser;
        HeaderBar header;
        std::unique_ptr<juce::Component> panel;
        TuneOverlay tuneOverlay;            // over browser and panel while tuning
        InactiveOverlay inactiveOverlay;
        bool browserVisible = true;
    };

    int designWidth() const noexcept;
    void setBrowserVisible (bool);
    void updateAspect();
    juce::String settingsKey (const char* name) const;

    bool isStandalone() const noexcept;

    ProgrammerProcessor& programmer;
    LookAndFeel lookAndFeel;
    juce::SharedResourcePointer<AppSettings> settings;
    Content content;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgrammerEditor)
};

} // namespace sso
