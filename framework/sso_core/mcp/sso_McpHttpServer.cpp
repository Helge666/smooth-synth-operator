namespace sso
{

namespace
{
    juce::String reasonPhrase (int status)
    {
        switch (status)
        {
            case 200: return "OK";
            case 202: return "Accepted";
            case 400: return "Bad Request";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 411: return "Length Required";
            case 413: return "Payload Too Large";
            case 431: return "Request Header Fields Too Large";
            default:  return "Error";
        }
    }

    bool isLoopbackName (const juce::String& name)
    {
        return name == "127.0.0.1" || name == "localhost" || name == "[::1]";
    }
}

//==============================================================================
/** One client connection: requests one after another (keep-alive) until the client closes it,
    it stays idle too long or the server stops. */
class McpHttpServer::Connection : public juce::Thread
{
public:
    Connection (McpHttpServer& o, std::unique_ptr<juce::StreamingSocket> s)
        : juce::Thread ("SSO MCP connection"), owner (o), socket (std::move (s))
    {
    }

    ~Connection() override
    {
        stopThread (3000);
    }

    std::atomic<bool> finished { false };

private:
    void run() override
    {
        for (;;)
        {
            auto request = readRequest();

            if (! request.has_value())
                break;

            Response response;

            if (problem != 0)
                response.status = problem;
            else
                response = owner.respond (*request);

            auto keepAlive = problem == 0 && ! request->headers["connection"].equalsIgnoreCase ("close");

            if (! write (response, keepAlive) || ! keepAlive || threadShouldExit())
                break;
        }

        socket->close();
        finished = true;
    }

    /** Waits for data; false on close, error, idle timeout or stop. */
    bool receive()
    {
        auto idleSince = juce::Time::getMillisecondCounter();

        while (! threadShouldExit())
        {
            auto ready = socket->waitUntilReady (true, 200);

            if (ready < 0)
                return false;

            if (ready > 0)
            {
                char chunk[8192];
                auto n = socket->read (chunk, (int) sizeof (chunk), false);

                if (n <= 0)
                    return false;

                pending.append (chunk, (size_t) n);
                return true;
            }

            if (juce::Time::getMillisecondCounter() - idleSince > (juce::uint32) idleTimeoutMs)
                return false;
        }

        return false;
    }

    std::optional<Request> readRequest()
    {
        problem = 0;
        int headerEnd = -1;

        for (;;)
        {
            auto text = juce::String::fromUTF8 ((const char*) pending.getData(), (int) pending.getSize());
            headerEnd = text.indexOf ("\r\n\r\n");

            if (headerEnd >= 0)
                break;

            if ((int) pending.getSize() > maxHeaderBytes)
            {
                problem = 431;
                return Request {};
            }

            if (! receive())
                return std::nullopt;
        }

        // Header bytes are ASCII, so the character index equals the byte index
        auto head = juce::String::fromUTF8 ((const char*) pending.getData(), headerEnd);
        auto lines = juce::StringArray::fromTokens (head, "\r\n", {});
        lines.removeEmptyStrings();

        Request r;
        auto requestLine = juce::StringArray::fromTokens (lines[0], " ", {});
        r.method = requestLine[0];
        r.path = requestLine[1].upToFirstOccurrenceOf ("?", false, false);

        for (int i = 1; i < lines.size(); ++i)
            r.headers.set (lines[i].upToFirstOccurrenceOf (":", false, false).trim().toLowerCase(),
                           lines[i].fromFirstOccurrenceOf (":", false, false).trim());

        auto bodyStart = (size_t) headerEnd + 4;

        if (r.headers["transfer-encoding"].isNotEmpty())
        {
            problem = 411;      // clients send JSON with a Content-Length
            return r;
        }

        auto length = r.headers["content-length"].getLargeIntValue();

        if (length < 0 || length > maxBodyBytes)
        {
            problem = 413;
            return r;
        }

        while (pending.getSize() < bodyStart + (size_t) length)
            if (! receive())
                return std::nullopt;

        r.body = juce::String::fromUTF8 ((const char*) pending.getData() + bodyStart, (int) length);

        // keep what follows (a pipelined next request)
        juce::MemoryBlock rest ((const char*) pending.getData() + bodyStart + (size_t) length,
                                pending.getSize() - bodyStart - (size_t) length);
        pending = rest;
        return r;
    }

    bool write (const Response& response, bool keepAlive)
    {
        auto body = response.body.toStdString();
        juce::String head;
        head << "HTTP/1.1 " << response.status << " " << reasonPhrase (response.status) << "\r\n";

        if (! body.empty())
            head << "Content-Type: application/json\r\n";

        head << "Content-Length: " << (int) body.size() << "\r\n"
             << "Connection: " << (keepAlive ? "keep-alive" : "close") << "\r\n";

        for (auto& key : response.headers.getAllKeys())
            head << key << ": " << response.headers[key] << "\r\n";

        head << "\r\n";
        auto data = head.toStdString() + body;
        return socket->write (data.data(), (int) data.size()) == (int) data.size();
    }

    McpHttpServer& owner;
    std::unique_ptr<juce::StreamingSocket> socket;
    juce::MemoryBlock pending;
    int problem = 0;        // HTTP status for a request that cannot be read
};

//==============================================================================
McpHttpServer::McpHttpServer (McpServer& server, int p)
    : juce::Thread ("SSO MCP server"), mcp (server), port (p)
{
}

McpHttpServer::~McpHttpServer()
{
    stop();
}

bool McpHttpServer::start()
{
    if (running.load())
        return true;

    if (! listener.createListener (port, "127.0.0.1"))
        return false;

    running = true;
    startThread();
    return true;
}

void McpHttpServer::stop()
{
    if (! running.exchange (false))
        return;

    signalThreadShouldExit();
    listener.close();       // ends waitForNextConnection
    stopThread (3000);

    const juce::ScopedLock sl (connectionLock);

    for (auto& c : connections)
        c->signalThreadShouldExit();

    connections.clear();    // each waits for its thread
}

void McpHttpServer::run()
{
    while (! threadShouldExit())
    {
        std::unique_ptr<juce::StreamingSocket> client (listener.waitForNextConnection());

        if (client == nullptr || threadShouldExit())
            break;

        removeFinishedConnections();
        const juce::ScopedLock sl (connectionLock);

        if ((int) connections.size() >= maxConnections)
            continue;       // closes the socket

        connections.push_back (std::make_unique<Connection> (*this, std::move (client)));
        connections.back()->startThread();
    }
}

void McpHttpServer::removeFinishedConnections()
{
    const juce::ScopedLock sl (connectionLock);
    connections.erase (std::remove_if (connections.begin(), connections.end(),
                                       [] (const std::unique_ptr<Connection>& c) { return c->finished.load(); }),
                       connections.end());
}

//==============================================================================
bool McpHttpServer::isAllowedHost (const juce::String& hostHeader) const
{
    if (hostHeader.isEmpty())
        return true;

    auto host = hostHeader.toLowerCase();
    auto hasPort = host.startsWithChar ('[') ? host.contains ("]:") : host.containsChar (':');
    auto name = hasPort ? host.upToLastOccurrenceOf (":", false, false) : host;

    if (! isLoopbackName (name))
        return false;

    return ! hasPort || host.fromLastOccurrenceOf (":", false, false).getIntValue() == port;
}

bool McpHttpServer::isAllowedOrigin (const juce::String& origin)
{
    auto o = origin.toLowerCase();

    for (auto scheme : { "http://", "https://" })
    {
        if (! o.startsWith (scheme))
            continue;

        auto host = o.fromFirstOccurrenceOf (scheme, false, false);
        auto hasPort = host.startsWithChar ('[') ? host.contains ("]:") : host.containsChar (':');
        return isLoopbackName (hasPort ? host.upToLastOccurrenceOf (":", false, false) : host);
    }

    return false;
}

juce::String McpHttpServer::decodeHeaderValue (const juce::String& value)
{
    // "=?base64?...?=" carries values that are not plain ASCII
    if (! (value.startsWith ("=?base64?") && value.endsWith ("?=")))
        return value;

    juce::MemoryOutputStream decoded;

    if (! juce::Base64::convertFromBase64 (decoded, value.substring (9, value.length() - 2)))
        return {};

    return decoded.toUTF8();
}

McpHttpServer::Response McpHttpServer::jsonResponse (const McpServer::Reply& reply)
{
    Response r;
    r.status = reply.httpStatus;

    if (! reply.isNotification())
        r.body = juce::JSON::toString (reply.message, true);

    return r;
}

McpHttpServer::Response McpHttpServer::respond (const Request& request) const
{
    auto error = [] (int status, int code, const juce::String& message, const juce::var& id = {})
    {
        return jsonResponse ({ McpServer::makeError (id, code, message), status });
    };

    if (! isAllowedHost (request.headers["host"]))
        return error (403, McpServer::invalidRequest, "Forbidden: only local connections");

    if (request.headers.containsKey ("origin") && ! isAllowedOrigin (request.headers["origin"]))
        return error (403, McpServer::invalidRequest, "Forbidden origin");

    if (request.path != "/mcp")
        return { 404, {}, {} };

    if (request.method != "POST")
    {
        Response r { 405, {}, {} };
        r.headers.set ("Allow", "POST");
        return r;
    }

    juce::var message;

    if (juce::JSON::parse (request.body, message).failed() || ! message.isObject())
        return error (400, McpServer::parseError, "Parse error: the body is not a JSON object");

    auto method = message["method"].toString();
    auto params = message["params"];
    auto bodyVersion = params["_meta"][juce::Identifier (juce::String (McpServer::metaPrefix) + "protocolVersion")].toString();
    auto headerVersion = request.headers["mcp-protocol-version"];

    if (bodyVersion.isNotEmpty() && message.hasProperty ("id"))
    {
        // Modern request: the mirrored headers must be there and match the body
        auto mismatch = [&] (const juce::String& what) { return error (400, McpServer::headerMismatch, "Header mismatch: " + what, message["id"]); };

        if (headerVersion != bodyVersion)
            return mismatch ("MCP-Protocol-Version must be " + bodyVersion);

        if (request.headers["mcp-method"] != method)
            return mismatch ("Mcp-Method must be " + method);

        if (method == "tools/call" || method == "prompts/get" || method == "resources/read")
        {
            auto name = method == "resources/read" ? params["uri"].toString() : params["name"].toString();

            if (decodeHeaderValue (request.headers["mcp-name"]) != name)
                return mismatch ("Mcp-Name must be " + name);
        }
    }
    else if (headerVersion.isNotEmpty() && ! McpServer::legacyVersions().contains (headerVersion)
             && ! McpServer::modernVersions().contains (headerVersion))
    {
        return error (400, McpServer::invalidRequest, "Unsupported MCP-Protocol-Version " + headerVersion, message["id"]);
    }

    return jsonResponse (mcp.handle (message));
}

} // namespace sso
