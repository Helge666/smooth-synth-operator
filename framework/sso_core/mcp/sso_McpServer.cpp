namespace sso
{

namespace json
{
    juce::var object (std::initializer_list<std::pair<const char*, juce::var>> properties)
    {
        auto* o = new juce::DynamicObject();

        for (auto& [key, value] : properties)
            o->setProperty (key, value);

        return juce::var (o);
    }

    juce::var array (std::initializer_list<juce::var> values)
    {
        juce::Array<juce::var> a;

        for (auto& v : values)
            a.add (v);

        return juce::var (a);
    }

    juce::var schema (std::initializer_list<std::pair<const char*, juce::var>> properties, juce::StringArray required)
    {
        auto s = object ({ { "type", "object" }, { "properties", object (properties) } });

        if (! required.isEmpty())
        {
            juce::Array<juce::var> r;

            for (auto& name : required)
                r.add (name);

            s.getDynamicObject()->setProperty ("required", r);
        }

        return s;
    }
}

//==============================================================================
const juce::StringArray& McpServer::modernVersions()
{
    static const juce::StringArray versions { "2026-07-28" };
    return versions;
}

const juce::StringArray& McpServer::legacyVersions()
{
    static const juce::StringArray versions { "2025-11-25", "2025-06-18", "2025-03-26" };
    return versions;
}

McpServer::McpServer (juce::String n, juce::String t, juce::String v, juce::String i)
    : name (std::move (n)), title (std::move (t)), version (std::move (v)), instructions (std::move (i))
{
}

McpServer::~McpServer()
{
    shutdown();
}

void McpServer::shutdown()
{
    *stopped = true;
}

void McpServer::addTool (Tool tool)
{
    tools.push_back (std::move (tool));
}

juce::var McpServer::makeError (const juce::var& id, int code, const juce::String& message, const juce::var& data)
{
    auto error = json::object ({ { "code", code }, { "message", message } });

    if (! data.isVoid())
        error.getDynamicObject()->setProperty ("data", data);

    auto reply = json::object ({ { "jsonrpc", "2.0" } });

    if (! id.isVoid())
        reply.getDynamicObject()->setProperty ("id", id);

    reply.getDynamicObject()->setProperty ("error", error);
    return reply;
}

juce::var McpServer::serverInfo() const
{
    return json::object ({ { "name", name }, { "title", title }, { "version", version } });
}

//==============================================================================
McpServer::Reply McpServer::handleJson (const juce::String& text)
{
    juce::var message;

    if (juce::JSON::parse (text, message).failed() || ! message.isObject())
        return { makeError ({}, parseError, "Parse error: the body is not a JSON object"), 400 };

    return handle (message);
}

McpServer::Reply McpServer::handle (const juce::var& message)
{
    if (! message.isObject() || message["jsonrpc"].toString() != "2.0")
        return { makeError (message["id"], invalidRequest, "Invalid request: not a JSON-RPC 2.0 message"), 400 };

    auto method = message["method"].toString();
    auto* object = message.getDynamicObject();

    if (method.isEmpty())
    {
        // A response from the client: this server never sends requests, so there is nothing to match
        return { {}, 202 };
    }

    if (! object->hasProperty ("id"))
        return { {}, 202 };     // notification (initialized, cancelled, ...): nothing to answer

    auto id = message["id"];

    if (! (id.isString() || id.isInt() || id.isInt64() || id.isDouble()))
        return { makeError ({}, invalidRequest, "Invalid request: the id must be a string or a number"), 400 };

    return handleRequest (id, method, message["params"]);
}

McpServer::Activity McpServer::getActivity() const
{
    const juce::ScopedLock sl (activityLock);
    return activity;
}

void McpServer::noteActivity (const juce::var& params, bool toolCall)
{
    // clientInfo: in initialize (legacy) or in every request's _meta (modern)
    auto info = params["clientInfo"];

    if (! info.isObject())
        info = params["_meta"][juce::Identifier (juce::String (metaPrefix) + "clientInfo")];

    const juce::ScopedLock sl (activityLock);
    auto now = juce::Time::getMillisecondCounterHiRes();
    activity.lastRequestMs = now;

    if (toolCall)
        activity.lastToolCallMs = now;

    if (info.isObject())
        activity.clientName = info["title"].toString().isNotEmpty() ? info["title"].toString() : info["name"].toString();
}

McpServer::Reply McpServer::handleRequest (const juce::var& id, const juce::String& method, const juce::var& params)
{
    noteActivity (params, method == "tools/call");

    auto meta = params["_meta"];
    auto requested = meta[juce::Identifier (juce::String (metaPrefix) + "protocolVersion")].toString();
    auto modern = requested.isNotEmpty();

    auto result = [&id, modern, this] (juce::var r) -> Reply
    {
        auto* o = r.getDynamicObject();
        o->setProperty ("resultType", "complete");

        if (modern)
            o->setProperty ("_meta", json::object ({ { "io.modelcontextprotocol/serverInfo", serverInfo() } }));

        return { json::object ({ { "jsonrpc", "2.0" }, { "id", id }, { "result", r } }), 200 };
    };

    auto supportedVersions = [] { juce::StringArray all (modernVersions()); all.addArray (legacyVersions()); return all; };

    if (modern)
    {
        if (! modernVersions().contains (requested))
        {
            juce::Array<juce::var> supported;

            for (auto& v : supportedVersions())
                supported.add (v);

            return { makeError (id, unsupportedProtocolVersion, "Unsupported protocol version",
                                json::object ({ { "supported", supported }, { "requested", requested } })), 400 };
        }

        if (! meta.hasProperty (juce::Identifier (juce::String (metaPrefix) + "clientCapabilities")))
            return { makeError (id, invalidParams, "Invalid params: _meta lacks io.modelcontextprotocol/clientCapabilities"), 400 };
    }

    if (method == "initialize")
        return result (initializeResult (params));

    if (method == "server/discover")
        return result (discoverResult());

    if (method == "ping")
        return result (json::object());

    if (method == "tools/list")
        return result (toolsListResult());

    if (method == "tools/call")
    {
        bool unknownTool = false;
        auto r = callTool (params, unknownTool);

        if (unknownTool)
            return { makeError (id, invalidParams, "Unknown tool: " + params["name"].toString()), modern ? 400 : 200 };

        return result (r);
    }

    return { makeError (id, methodNotFound, "Method not found: " + method), modern ? 404 : 200 };
}

juce::var McpServer::initializeResult (const juce::var& params) const
{
    // Legacy handshake: answer with the client's version if we speak it, else our newest legacy one
    auto requested = params["protocolVersion"].toString();
    auto agreed = legacyVersions().contains (requested) ? requested : legacyVersions()[0];

    return json::object ({ { "protocolVersion", agreed },
                           { "capabilities", json::object ({ { "tools", json::object ({ { "listChanged", false } }) } }) },
                           { "serverInfo", serverInfo() },
                           { "instructions", instructions } });
}

juce::var McpServer::discoverResult() const
{
    juce::Array<juce::var> supported;

    for (auto& v : modernVersions())
        supported.add (v);

    for (auto& v : legacyVersions())
        supported.add (v);

    return json::object ({ { "supportedVersions", supported },
                           { "capabilities", json::object ({ { "tools", json::object ({ { "listChanged", false } }) } }) },
                           { "instructions", instructions },
                           { "ttlMs", 60000 },
                           { "cacheScope", "private" } });
}

juce::var McpServer::toolsListResult() const
{
    juce::Array<juce::var> list;

    for (auto& t : tools)
    {
        auto annotations = json::object ({ { "title", t.title },
                                           { "readOnlyHint", t.readOnly },
                                           { "destructiveHint", t.destructive },
                                           { "openWorldHint", false } });

        list.add (json::object ({ { "name", t.name }, { "title", t.title }, { "description", t.description },
                                  { "inputSchema", t.inputSchema }, { "annotations", annotations } }));
    }

    return json::object ({ { "tools", list }, { "ttlMs", 60000 }, { "cacheScope", "private" } });
}

juce::var McpServer::callTool (const juce::var& params, bool& unknownTool)
{
    auto toolName = params["name"].toString();
    auto it = std::find_if (tools.begin(), tools.end(), [&toolName] (const Tool& t) { return t.name == toolName; });

    if (it == tools.end())
    {
        unknownTool = true;
        return {};
    }

    auto arguments = params["arguments"];

    if (arguments.isVoid())
        arguments = json::object();

    ToolResult r;

    if (! arguments.isObject())
        r = ToolResult::error ("The arguments must be a JSON object.");
    else
        r = runOnMessageThread (*it, arguments);

    auto content = json::array ({ json::object ({ { "type", "text" }, { "text", r.text } }) });
    auto out = json::object ({ { "content", content }, { "isError", r.isError } });

    if (! r.structured.isVoid())
        out.getDynamicObject()->setProperty ("structuredContent", r.structured);

    return out;
}

McpServer::ToolResult McpServer::runOnMessageThread (const Tool& tool, const juce::var& arguments)
{
    if (stopped->load())
        return ToolResult::error ("The app is closing.");

    auto* mm = juce::MessageManager::getInstanceWithoutCreating();

    if (mm == nullptr || mm->isThisTheMessageThread())
        return tool.call (arguments);

    // Wait for the message thread, but never forever: the app may be closing
    struct Job
    {
        juce::WaitableEvent done;
        ToolResult result;
    };

    auto job = std::make_shared<Job>();
    auto call = tool.call;

    juce::MessageManager::callAsync ([job, call, arguments, stop = stopped]
    {
        // the tool's objects may be gone once the server was shut down
        job->result = stop->load() ? ToolResult::error ("The app is closing.") : call (arguments);
        job->done.signal();
    });

    while (! job->done.wait (100))
        if (stopped->load())
            return ToolResult::error ("The app is closing.");

    return job->result;
}

} // namespace sso
