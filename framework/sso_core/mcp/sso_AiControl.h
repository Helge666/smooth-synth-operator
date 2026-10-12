#pragma once

namespace sso
{

/** AI control (MCP) for one programmer instance: runs the tools and the local HTTP endpoint
    while the user has switched it on (remembered per synth for all instances) and this
    instance controls the synth. Off by default.

    Each synth has its own port (DeviceDefinition::getMcpPort), so the apps can run side by
    side; of several instances for one synth only the active one answers.
*/
class AiControl : public juce::ChangeBroadcaster,
                  private juce::ChangeListener,
                  private juce::Timer
{
public:
    struct Setup
    {
        juce::String appName;                               // "SSO Hawk-800"
        std::function<juce::String()> appVersion;           // asked when the server starts
        std::function<PatchGenerator*()> generator;
    };

    AiControl (SynthController&, PresetLibrary&, Setup);
    ~AiControl() override;

    bool isEnabled() const;
    void setEnabled (bool);

    enum class Status { off, running, portBusy, inactive };
    Status getStatus() const noexcept               { return status; }
    int getPort() const;
    juce::String getUrl() const                     { return "http://127.0.0.1:" + juce::String (getPort()) + "/mcp"; }

    /** Who used it last; empty while not running. */
    McpServer::Activity getActivity() const;

    /** One line for the user, e.g. "Running at http://127.0.0.1:47833/mcp - last used by ...". */
    juce::String describeStatus() const;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override  { update(); }
    void timerCallback() override                                     { update(); }
    void update();
    void startServer();
    void stopServer();
    juce::String settingsKey (const char* name) const;

    SynthController& controller;
    PresetLibrary& library;
    Setup setup;
    juce::SharedResourcePointer<AppSettings> settings;

    std::unique_ptr<SynthTools> tools;
    std::unique_ptr<McpServer> server;
    std::unique_ptr<McpHttpServer> http;
    Status status = Status::off;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AiControl)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AiControl)
};

} // namespace sso
