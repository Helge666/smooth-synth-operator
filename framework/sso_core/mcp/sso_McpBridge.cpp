namespace sso
{

McpBridge::McpBridge (int p, juce::String name) : port (p), appName (std::move (name))
{
}

juce::String McpBridge::unreachable() const
{
    return appName + " is not reachable at " + getUrl() + ". Start the app (or open the Cubase project that uses it) "
           "and switch on AI control in its MIDI window, then try again.";
}

juce::StringPairArray McpBridge::headersFor (const juce::var& message)
{
    juce::StringPairArray headers;
    auto params = message["params"];
    auto version = params["_meta"][juce::Identifier (juce::String (McpServer::metaPrefix) + "protocolVersion")].toString();

    if (version.isEmpty())
        return headers;     // legacy messages carry no mirrored headers

    auto method = message["method"].toString();
    headers.set ("MCP-Protocol-Version", version);
    headers.set ("Mcp-Method", method);

    if (method == "tools/call" || method == "prompts/get" || method == "resources/read")
    {
        auto name = method == "resources/read" ? params["uri"].toString() : params["name"].toString();
        auto plain = name.isNotEmpty() && name == name.trim() && ! (name.startsWith ("=?base64?") && name.endsWith ("?="));

        for (auto c : name)
            plain = plain && c >= 0x20 && c < 0x7f;

        headers.set ("Mcp-Name", plain ? name : "=?base64?" + juce::Base64::toBase64 (name) + "?=");
    }

    return headers;
}

juce::String McpBridge::handleLine (const juce::String& line)
{
    if (line.trim().isEmpty())
        return {};

    juce::var message;

    if (juce::JSON::parse (line, message).failed() || ! message.isObject())
        return juce::JSON::toString (McpServer::makeError ({}, McpServer::parseError, "Parse error"), true);

    auto isRequest = message.getDynamicObject()->hasProperty ("id") && message["method"].toString().isNotEmpty();
    auto result = post (line, headersFor (message));

    if (result.status == 0)
    {
        if (! isRequest)
            return {};

        if (message["method"].toString() == "tools/call")
        {
            // as a tool error, so the AI can tell the user what to do
            auto content = json::array ({ json::object ({ { "type", "text" }, { "text", unreachable() } }) });
            return juce::JSON::toString (json::object ({ { "jsonrpc", "2.0" }, { "id", message["id"] },
                                                         { "result", json::object ({ { "content", content }, { "isError", true } }) } }), true);
        }

        return juce::JSON::toString (McpServer::makeError (message["id"], McpServer::internalError, unreachable()), true);
    }

    if (! isRequest || result.body.isEmpty())
        return {};

    // one message per line: the app writes JSON on one line, but make sure
    juce::var reply;

    if (juce::JSON::parse (result.body, reply).failed())
        return juce::JSON::toString (McpServer::makeError (message["id"], McpServer::internalError,
                                                           "The app sent an unreadable answer (HTTP " + juce::String (result.status) + ")"), true);

    return juce::JSON::toString (reply, true);
}

McpBridge::HttpResult McpBridge::post (const juce::String& body, const juce::StringPairArray& headers)
{
    auto bytes = body.toStdString();
    juce::String head;
    head << "POST /mcp HTTP/1.1\r\n"
         << "Host: 127.0.0.1:" << port << "\r\n"
         << "Content-Type: application/json\r\n"
         << "Accept: application/json, text/event-stream\r\n";

    for (auto& key : headers.getAllKeys())
        head << key << ": " << headers[key] << "\r\n";

    head << "Content-Length: " << (int) bytes.size() << "\r\n\r\n";
    auto request = head.toStdString() + bytes;

    // An idle connection may have been closed by the app: one more try on a new one
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        if (socket == nullptr || ! socket->isConnected())
        {
            socket = std::make_unique<juce::StreamingSocket>();

            if (! socket->connect ("127.0.0.1", port, 1000))
            {
                socket.reset();
                return {};
            }
        }

        auto result = exchange (request);

        if (result.status != 0)
            return result;

        socket.reset();
    }

    return {};
}

McpBridge::HttpResult McpBridge::exchange (const std::string& request)
{
    if (socket->write (request.data(), (int) request.size()) != (int) request.size())
        return {};

    std::string received;
    size_t headerEnd = std::string::npos, needed = 0;
    auto start = juce::Time::getMillisecondCounter();

    // tools answer quickly; 60 s covers a busy message thread
    while (juce::Time::getMillisecondCounter() - start < 60000)
    {
        auto ready = socket->waitUntilReady (true, 200);

        if (ready < 0)
            return {};

        if (ready > 0)
        {
            char buffer[8192];
            auto n = socket->read (buffer, (int) sizeof (buffer), false);

            if (n <= 0)
                return {};

            received.append (buffer, (size_t) n);
        }

        if (headerEnd == std::string::npos && (headerEnd = received.find ("\r\n\r\n")) != std::string::npos)
        {
            auto headText = juce::String (received.substr (0, headerEnd)).toLowerCase();
            needed = headerEnd + 4 + (size_t) headText.fromFirstOccurrenceOf ("content-length:", false, false).getIntValue();
        }

        if (headerEnd != std::string::npos && received.size() >= needed)
        {
            HttpResult r;
            r.status = juce::String (received.substr (9, 3)).getIntValue();
            r.body = juce::String::fromUTF8 (received.data() + headerEnd + 4, (int) (needed - headerEnd - 4));

            if (juce::String (received.substr (0, headerEnd)).containsIgnoreCase ("connection: close"))
                socket.reset();

            return r;
        }
    }

    return {};
}

} // namespace sso
