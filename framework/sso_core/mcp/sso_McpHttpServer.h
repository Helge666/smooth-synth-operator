#pragma once

namespace sso
{

/** The MCP "Streamable HTTP" transport: a minimal HTTP/1.1 server on 127.0.0.1 with a single
    endpoint, POST /mcp, answered with application/json (no event streams, no sessions).

    Only programs on this computer can connect: it listens on the loopback address and
    rejects requests whose Host or Origin header names another site (DNS rebinding).
    For modern requests the mirrored headers (MCP-Protocol-Version, Mcp-Method, Mcp-Name)
    must match the body.
*/
class McpHttpServer : private juce::Thread
{
public:
    McpHttpServer (McpServer&, int port);
    ~McpHttpServer() override;

    /** False if the port is taken (another program, or another SSO app). */
    bool start();
    void stop();
    bool isRunning() const noexcept     { return running.load(); }
    int getPort() const noexcept        { return port; }
    juce::String getUrl() const         { return "http://127.0.0.1:" + juce::String (port) + "/mcp"; }

    struct Request
    {
        juce::String method, path;
        juce::StringPairArray headers;      // names in lower case
        juce::String body;
    };

    struct Response
    {
        int status = 200;
        juce::String body;                  // JSON, or empty
        juce::StringPairArray headers;
    };

    /** The whole HTTP-level logic, without sockets (also for tests). */
    Response respond (const Request&) const;

    static constexpr int maxHeaderBytes = 32 * 1024;
    static constexpr int maxBodyBytes = 4 * 1024 * 1024;
    static constexpr int maxConnections = 16;
    static constexpr int idleTimeoutMs = 30000;

private:
    class Connection;

    void run() override;
    void removeFinishedConnections();
    bool isAllowedHost (const juce::String& host) const;
    static bool isAllowedOrigin (const juce::String& origin);
    static juce::String decodeHeaderValue (const juce::String&);
    static Response jsonResponse (const McpServer::Reply&);

    McpServer& mcp;
    const int port;
    juce::StreamingSocket listener;
    std::atomic<bool> running { false };
    juce::CriticalSection connectionLock;
    std::vector<std::unique_ptr<Connection>> connections;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (McpHttpServer)
};

} // namespace sso
