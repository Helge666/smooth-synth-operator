#pragma once

namespace sso
{

/** The Model Context Protocol (MCP) side of "AI control": JSON-RPC messages in, replies out,
    independent of the transport (HTTP in the app, stdio through the sso-mcp bridge).

    Speaks both protocol eras:
    - modern (2026-07-28): stateless, every request carries its protocol version and client
      capabilities in params._meta; server/discover describes the server.
    - legacy (2025-03-26 ... 2025-11-25): an initialize handshake, then requests without
      per-request metadata. Today's clients mostly speak this.

    Only tools are offered. Tool functions run on the message thread; handle() may be called
    from any thread and waits for them.
*/
class McpServer
{
public:
    struct ToolResult
    {
        juce::String text;              // what the AI reads
        juce::var structured;           // optional machine-readable result (structuredContent)
        bool isError = false;           // a tool-level failure the AI can correct

        static ToolResult error (const juce::String& message)   { return { message, {}, true }; }
    };

    struct Tool
    {
        juce::String name, title, description;
        juce::var inputSchema;          // JSON Schema of the arguments (type: object)
        bool readOnly = false;          // changes nothing (hint for clients)
        bool destructive = false;       // may overwrite something the user has
        std::function<ToolResult (const juce::var& arguments)> call;
    };

    McpServer (juce::String name, juce::String title, juce::String version, juce::String instructions);
    ~McpServer();

    void addTool (Tool);
    const std::vector<Tool>& getTools() const noexcept   { return tools; }

    /** The reply to one message: a JSON-RPC response, or void for a notification, plus the
        HTTP status the Streamable HTTP binding prescribes for it. */
    struct Reply
    {
        juce::var message;
        int httpStatus = 200;
        bool isNotification() const     { return message.isVoid(); }
    };

    Reply handle (const juce::var& message);
    Reply handleJson (const juce::String& json);

    /** Stops waiting for tool calls (tools still queued are answered with an error) -
        call before the objects the tools use go away. */
    void shutdown();

    static const juce::StringArray& modernVersions();
    static const juce::StringArray& legacyVersions();
    static constexpr const char* metaPrefix = "io.modelcontextprotocol/";

    // JSON-RPC and MCP error codes
    enum ErrorCode
    {
        parseError = -32700, invalidRequest = -32600, methodNotFound = -32601, invalidParams = -32602,
        internalError = -32603, headerMismatch = -32020, unsupportedProtocolVersion = -32022
    };

    static juce::var makeError (const juce::var& id, int code, const juce::String& message, const juce::var& data = {});

private:
    Reply handleRequest (const juce::var& id, const juce::String& method, const juce::var& params);
    juce::var initializeResult (const juce::var& params) const;
    juce::var discoverResult() const;
    juce::var toolsListResult() const;
    juce::var callTool (const juce::var& params, bool& unknownTool);
    ToolResult runOnMessageThread (const Tool&, const juce::var& arguments);
    juce::var serverInfo() const;

    const juce::String name, title, version, instructions;
    std::vector<Tool> tools;
    std::shared_ptr<std::atomic<bool>> stopped = std::make_shared<std::atomic<bool>> (false);   // shared with queued calls

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (McpServer)
};

/** Small helpers for building JSON values. */
namespace json
{
    juce::var object (std::initializer_list<std::pair<const char*, juce::var>> = {});
    juce::var array (std::initializer_list<juce::var> = {});
    juce::var schema (std::initializer_list<std::pair<const char*, juce::var>> properties = {},
                      juce::StringArray required = {});
}

} // namespace sso
