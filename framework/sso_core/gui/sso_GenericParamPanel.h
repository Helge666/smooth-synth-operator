#pragma once

namespace sso
{

/** Auto-generated editor for all parameters of a device, grouped by section.
    A functional fallback/debug view; device-specific panels replace it later. */
class GenericParamPanel : public juce::Component
{
public:
    GenericParamPanel (const DeviceDefinition&, juce::AudioProcessorValueTreeState&);
    ~GenericParamPanel() override;

    void resized() override;

    /** Height needed to show everything at the given width. */
    int getRequiredHeight (int width) const;

private:
    struct ParamControl;
    struct Section;

    int layoutSections (int width, bool apply) const;

    std::vector<std::unique_ptr<Section>> sections;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GenericParamPanel)
};

} // namespace sso
