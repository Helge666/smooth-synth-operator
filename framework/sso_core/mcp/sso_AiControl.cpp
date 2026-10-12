namespace sso
{

AiControl::AiControl (SynthController& c, PresetLibrary& l, Setup s)
    : controller (c), library (l), setup (std::move (s))
{
    controller.addChangeListener (this);

    // First look once the programmer is complete (its generator is created after this)
    juce::MessageManager::callAsync ([safe = juce::WeakReference<AiControl> (this)]
    {
        if (safe != nullptr)
            safe->update();
    });
}

AiControl::~AiControl()
{
    stopTimer();
    controller.removeChangeListener (this);
    stopServer();
}

juce::String AiControl::settingsKey (const char* name) const
{
    return controller.getDevice().getDeviceId() + "." + name;
}

bool AiControl::isEnabled() const
{
    return settings->get().getBoolValue (settingsKey ("aiControl"), false);
}

void AiControl::setEnabled (bool shouldBeEnabled)
{
    settings->get().setValue (settingsKey ("aiControl"), shouldBeEnabled);
    update();
}

int AiControl::getPort() const
{
    return settings->get().getIntValue (settingsKey ("mcpPort"), controller.getDevice().getMcpPort());
}

McpServer::Activity AiControl::getActivity() const
{
    return server != nullptr ? server->getActivity() : McpServer::Activity();
}

void AiControl::update()
{
    auto before = status;
    auto wanted = isEnabled() && controller.isActive();

    if (wanted && http == nullptr)
        startServer();
    else if (! wanted && http != nullptr)
        stopServer();

    status = ! isEnabled() ? Status::off
           : ! controller.isActive() ? Status::inactive
           : http != nullptr ? Status::running
           : Status::portBusy;

    // A taken port may become free: try again now and then
    if (status == Status::portBusy)
    {
        if (! isTimerRunning())
            startTimer (5000);
    }
    else
    {
        stopTimer();
    }

    if (status != before)
        sendChangeMessage();
}

void AiControl::startServer()
{
    tools = std::make_unique<SynthTools> (SynthTools::Context { setup.appName, setup.appVersion != nullptr ? setup.appVersion() : juce::String(),
                                                                controller, library,
                                                                setup.generator != nullptr ? setup.generator() : nullptr });
    server = std::make_unique<McpServer> ("sso-" + controller.getDevice().getDeviceId(), setup.appName,
                                          setup.appVersion != nullptr ? setup.appVersion() : juce::String(), tools->instructions());
    tools->addTo (*server);
    http = std::make_unique<McpHttpServer> (*server, getPort());

    if (! http->start())
        stopServer();
}

void AiControl::stopServer()
{
    // Tool calls waiting for the message thread give up first, then the connections close
    if (server != nullptr)
        server->shutdown();

    http.reset();
    server.reset();
    tools.reset();
}

juce::String AiControl::describeStatus() const
{
    switch (status)
    {
        case Status::off:       return "Off";
        case Status::inactive:  return "Waiting: another SSO instance controls the synth";
        case Status::portBusy:  return "Port " + juce::String (getPort()) + " is used by another program - trying again";
        case Status::running:   break;
    }

    auto text = "Running at " + getUrl();
    auto activity = getActivity();

    if (activity.lastRequestMs > 0)
    {
        auto seconds = (juce::Time::getMillisecondCounterHiRes() - activity.lastRequestMs) / 1000.0;
        text << " - last used by " << (activity.clientName.isNotEmpty() ? activity.clientName : juce::String ("a client"))
             << (seconds < 60 ? " " + juce::String ((int) seconds) + " s ago"
                              : " " + juce::String ((int) (seconds / 60)) + " min ago");
    }

    return text;
}

} // namespace sso
