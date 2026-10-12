#pragma once

namespace sso
{

/** Connects an MCP client that starts its servers as programs (stdio, e.g. Claude Desktop)
    with an app's local HTTP endpoint: every line from the client is POSTed to the app,
    every reply becomes a line back. The logic lives in the app; this only forwards.

    If the app cannot be reached, requests get a readable answer (tool calls as a tool error
    the AI can pass on) instead of failing silently.
*/
class McpBridge
{
public:
    McpBridge (int port, juce::String appName);

    /** One JSON-RPC message from the client; returns the line to send back (empty = none). */
    juce::String handleLine (const juce::String& line);

    /** The headers the Streamable HTTP binding mirrors from a message's body. */
    static juce::StringPairArray headersFor (const juce::var& message);

    juce::String getUrl() const     { return "http://127.0.0.1:" + juce::String (port) + "/mcp"; }

private:
    struct HttpResult
    {
        int status = 0;             // 0 = the app could not be reached
        juce::String body;
    };

    HttpResult post (const juce::String& body, const juce::StringPairArray& headers);
    HttpResult exchange (const std::string& request);
    juce::String unreachable() const;

    const int port;
    const juce::String appName;
    std::unique_ptr<juce::StreamingSocket> socket;      // kept open between messages (keep-alive)
};

} // namespace sso
