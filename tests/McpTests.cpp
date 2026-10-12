#include <sso_hawk800/sso_hawk800.h>
#include <sso_kiwi3p/sso_kiwi3p.h>
#include <sso_kiwisix/sso_kiwisix.h>

namespace
{
    struct McpDummyProcessor final : public juce::AudioProcessor
    {
        const juce::String getName() const override { return "test"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return true; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    juce::var parse (const juce::String& s)    { return juce::JSON::parse (s); }

    juce::String modernMeta (const juce::String& version = "2026-07-28", bool withCapabilities = true)
    {
        return "\"_meta\": { \"io.modelcontextprotocol/protocolVersion\": \"" + version + "\""
               + (withCapabilities ? ", \"io.modelcontextprotocol/clientCapabilities\": {}" : "") + " }";
    }
}

class McpTests final : public juce::UnitTest
{
public:
    McpTests() : juce::UnitTest ("AI control (MCP)", "SSO") {}

    void runTest() override
    {
        protocol();
        tools();
        http();
    }

    /** One HTTP exchange on an open socket; returns status and body. */
    static std::pair<int, juce::String> exchange (juce::StreamingSocket& socket, const juce::String& request)
    {
        auto data = request.toStdString();
        socket.write (data.data(), (int) data.size());

        std::string received;
        size_t headerEnd = std::string::npos, needed = 0;

        for (int i = 0; i < 200; ++i)
        {
            if (socket.waitUntilReady (true, 20) > 0)
            {
                char buffer[4096];
                auto n = socket.read (buffer, sizeof (buffer), false);

                if (n <= 0)
                    break;

                received.append (buffer, (size_t) n);
            }
            else
            {
                juce::MessageManager::getInstance()->runDispatchLoopUntil (5);   // tools run on this thread
            }

            if (headerEnd == std::string::npos && (headerEnd = received.find ("\r\n\r\n")) != std::string::npos)
            {
                auto head = juce::String (received.substr (0, headerEnd));
                auto length = head.fromFirstOccurrenceOf ("Content-Length: ", false, true).getIntValue();
                needed = headerEnd + 4 + (size_t) length;
            }

            if (headerEnd != std::string::npos && received.size() >= needed)
                break;
        }

        if (headerEnd == std::string::npos)
            return { 0, {} };

        auto status = juce::String (received.substr (9, 3)).getIntValue();
        return { status, juce::String::fromUTF8 (received.data() + headerEnd + 4, (int) (received.size() - headerEnd - 4)) };
    }

    static juce::String post (const juce::String& body, int port)
    {
        return "POST /mcp HTTP/1.1\r\nHost: 127.0.0.1:" + juce::String (port) + "\r\nContent-Type: application/json\r\n"
               "Accept: application/json, text/event-stream\r\n"
               "Content-Length: " + juce::String ((int) body.getNumBytesAsUTF8()) + "\r\n\r\n" + body;
    }

    void http()
    {
        sso::McpServer server ("sso-test", "SSO Test", "1.0", "");
        server.addTool ({ "echo", "Echo", "", sso::json::schema(), true, false,
                          [] (const juce::var& a) { return sso::McpServer::ToolResult { "echo " + a["text"].toString() }; } });

        auto port = 47900 + juce::Random::getSystemRandom().nextInt (90);
        sso::McpHttpServer http (server, port);

        beginTest ("HTTP rules: local only, POST /mcp, modern headers must match the body");
        {
            using Request = sso::McpHttpServer::Request;
            auto request = [port] (juce::String body, std::initializer_list<std::pair<const char*, const char*>> headers = {})
            {
                Request r { "POST", "/mcp", {}, body };
                r.headers.set ("host", "127.0.0.1:" + juce::String (port));

                for (auto& [k, v] : headers)
                    r.headers.set (k, v);

                return r;
            };

            const juce::String init = R"({ "jsonrpc": "2.0", "id": 1, "method": "initialize", "params": { "protocolVersion": "2025-06-18" } })";
            expectEquals (http.respond (request (init)).status, 200);
            expectEquals (http.respond (request (init, { { "host", "evil.example.com" } })).status, 403);
            expectEquals (http.respond (request (init, { { "host", "localhost:1" } })).status, 403, "another port");
            expectEquals (http.respond (request (init, { { "origin", "https://evil.example.com" } })).status, 403);
            expectEquals (http.respond (request (init, { { "origin", "http://localhost:3000" } })).status, 200);

            auto get = request ({});
            get.method = "GET";
            expectEquals (http.respond (get).status, 405);
            auto wrongPath = request (init);
            wrongPath.path = "/other";
            expectEquals (http.respond (wrongPath).status, 404);

            expectEquals (http.respond (request (R"({ "jsonrpc": "2.0", "method": "notifications/initialized" })")).status, 202);
            expectEquals (http.respond (request ("{ nonsense")).status, 400);

            const juce::String modern = R"({ "jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": { "name": "echo", "arguments": { "text": "a" },
                                             "_meta": { "io.modelcontextprotocol/protocolVersion": "2026-07-28", "io.modelcontextprotocol/clientCapabilities": {} } } })";
            auto ok = http.respond (request (modern, { { "mcp-protocol-version", "2026-07-28" }, { "mcp-method", "tools/call" }, { "mcp-name", "echo" } }));
            expectEquals (ok.status, 200);
            expect (ok.body.contains ("echo a"), ok.body);

            auto encoded = http.respond (request (modern, { { "mcp-protocol-version", "2026-07-28" }, { "mcp-method", "tools/call" },
                                                            { "mcp-name", "=?base64?ZWNobw==?=" } }));
            expectEquals (encoded.status, 200, "base64 Mcp-Name");

            auto missing = http.respond (request (modern, { { "mcp-protocol-version", "2026-07-28" }, { "mcp-method", "tools/call" } }));
            expectEquals (missing.status, 400);
            expect (missing.body.contains ("-32020"), missing.body);
            expectEquals (http.respond (request (modern, { { "mcp-protocol-version", "2025-11-25" }, { "mcp-method", "tools/call" },
                                                           { "mcp-name", "echo" } })).status, 400, "version header differs");
        }

        beginTest ("HTTP over a real socket: keep-alive, tool calls, port taken");
        {
            expect (http.start());

            juce::StreamingSocket client;
            expect (client.connect ("127.0.0.1", port, 2000));

            auto first = exchange (client, post (R"({ "jsonrpc": "2.0", "id": 1, "method": "initialize", "params": { "protocolVersion": "2025-11-25", "clientInfo": { "name": "test-client", "version": "1" } } })", port));
            expectEquals (first.first, 200);
            expect (first.second.contains ("\"protocolVersion\": \"2025-11-25\""), first.second);

            auto second = exchange (client, post (juce::String::fromUTF8 (u8R"({ "jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": { "name": "echo", "arguments": { "text": "über" } } })"), port));
            expectEquals (second.first, 200, "same connection");
            expect (second.second.contains (juce::String::fromUTF8 (u8"echo über")), second.second);
            expectEquals (server.getActivity().clientName, juce::String ("test-client"));
            expect (server.getActivity().lastToolCallMs > 0);

            auto third = exchange (client, post (R"({ "jsonrpc": "2.0", "method": "notifications/initialized" })", port));
            expectEquals (third.first, 202);
            expect (third.second.isEmpty());

            sso::McpHttpServer other (server, port);
            expect (! other.start(), "the port is taken");

            client.close();
            http.stop();

            juce::StreamingSocket late;
            expect (! late.connect ("127.0.0.1", port, 300), "stopped");
        }
    }

    void protocol()
    {
        sso::McpServer server ("sso-test", "SSO Test", "1.2.3", "Use the echo tool.");
        bool calledOnMessageThread = false;

        server.addTool ({ "echo", "Echo", "Returns its text.", sso::json::schema ({ { "text", sso::json::object ({ { "type", "string" } }) } }),
                          true, false, [&] (const juce::var& args)
                          {
                              calledOnMessageThread = juce::MessageManager::getInstance()->isThisTheMessageThread();
                              return sso::McpServer::ToolResult { "echo: " + args["text"].toString() };
                          } });

        beginTest ("Legacy clients: initialize handshake, tools/list, tools/call");
        {
            auto reply = server.handleJson (R"({ "jsonrpc": "2.0", "id": 1, "method": "initialize",
                                                 "params": { "protocolVersion": "2025-06-18", "capabilities": {},
                                                             "clientInfo": { "name": "c", "version": "1" } } })");
            auto r = reply.message["result"];
            expectEquals (reply.httpStatus, 200);
            expectEquals (r["protocolVersion"].toString(), juce::String ("2025-06-18"), "the client's version");
            expect (r["capabilities"]["tools"].isObject());
            expectEquals (r["serverInfo"]["name"].toString(), juce::String ("sso-test"));
            expectEquals (r["instructions"].toString(), juce::String ("Use the echo tool."));

            reply = server.handleJson (R"({ "jsonrpc": "2.0", "id": 2, "method": "initialize", "params": { "protocolVersion": "2024-11-05" } })");
            expectEquals (reply.message["result"]["protocolVersion"].toString(), juce::String ("2025-11-25"), "unknown -> our newest legacy");

            reply = server.handleJson (R"({ "jsonrpc": "2.0", "method": "notifications/initialized" })");
            expect (reply.isNotification() && reply.httpStatus == 202);

            reply = server.handleJson (R"({ "jsonrpc": "2.0", "id": "a", "method": "tools/list" })");
            auto list = reply.message["result"]["tools"];
            expect (list.isArray() && list.size() == 1);
            expectEquals (list[0]["name"].toString(), juce::String ("echo"));
            expectEquals (list[0]["inputSchema"]["type"].toString(), juce::String ("object"));
            expect ((bool) list[0]["annotations"]["readOnlyHint"]);
            expectEquals (reply.message["id"].toString(), juce::String ("a"), "string ids come back");

            reply = server.handleJson (R"({ "jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": { "name": "echo", "arguments": { "text": "hi" } } })");
            r = reply.message["result"];
            expectEquals (r["content"][0]["type"].toString(), juce::String ("text"));
            expectEquals (r["content"][0]["text"].toString(), juce::String ("echo: hi"));
            expect (! (bool) r["isError"]);
            expect (calledOnMessageThread);

            reply = server.handleJson (R"({ "jsonrpc": "2.0", "id": 4, "method": "tools/call", "params": { "name": "nope" } })");
            expectEquals ((int) reply.message["error"]["code"], (int) sso::McpServer::invalidParams);

            reply = server.handleJson (R"({ "jsonrpc": "2.0", "id": 5, "method": "resources/list" })");
            expectEquals ((int) reply.message["error"]["code"], (int) sso::McpServer::methodNotFound);
            expectEquals (reply.httpStatus, 200);

            reply = server.handleJson (R"({ "jsonrpc": "2.0", "id": 6, "method": "ping" })");
            expect (reply.message["result"].isObject());

            reply = server.handleJson ("not json");
            expectEquals ((int) reply.message["error"]["code"], (int) sso::McpServer::parseError);
        }

        beginTest ("Modern clients: per-request metadata, server/discover, version errors");
        {
            auto reply = server.handleJson (R"({ "jsonrpc": "2.0", "id": 1, "method": "server/discover", "params": { )" + modernMeta() + " } }");
            auto r = reply.message["result"];
            expectEquals (r["resultType"].toString(), juce::String ("complete"));
            expect (r["supportedVersions"].isArray());
            expect (r["supportedVersions"].getArray()->contains ("2026-07-28") && r["supportedVersions"].getArray()->contains ("2025-11-25"));
            expect (r["capabilities"]["tools"].isObject());
            expect (r["ttlMs"].isInt() && r["cacheScope"].toString() == "private");
            expectEquals (r["_meta"]["io.modelcontextprotocol/serverInfo"]["version"].toString(), juce::String ("1.2.3"));

            reply = server.handleJson (R"({ "jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": { "name": "echo", "arguments": { "text": "x" }, )"
                                       + modernMeta() + " } }");
            expectEquals (reply.message["result"]["content"][0]["text"].toString(), juce::String ("echo: x"));
            expectEquals (reply.message["result"]["resultType"].toString(), juce::String ("complete"));

            reply = server.handleJson (R"({ "jsonrpc": "2.0", "id": 3, "method": "tools/list", "params": { )" + modernMeta ("2099-01-01") + " } }");
            expectEquals (reply.httpStatus, 400);
            expectEquals ((int) reply.message["error"]["code"], (int) sso::McpServer::unsupportedProtocolVersion);
            expect (reply.message["error"]["data"]["supported"].getArray()->contains ("2026-07-28"));
            expectEquals (reply.message["error"]["data"]["requested"].toString(), juce::String ("2099-01-01"));

            reply = server.handleJson (R"({ "jsonrpc": "2.0", "id": 4, "method": "tools/list", "params": { )" + modernMeta ("2026-07-28", false) + " } }");
            expectEquals (reply.httpStatus, 400);
            expectEquals ((int) reply.message["error"]["code"], (int) sso::McpServer::invalidParams);

            reply = server.handleJson (R"({ "jsonrpc": "2.0", "id": 5, "method": "prompts/list", "params": { )" + modernMeta() + " } }");
            expectEquals (reply.httpStatus, 404);
            expectEquals ((int) reply.message["error"]["code"], (int) sso::McpServer::methodNotFound);
        }

        beginTest ("Calls from another thread run on the message thread");
        {
            calledOnMessageThread = false;
            std::atomic<bool> finished { false };
            juce::String text;

            std::thread worker ([&]
            {
                auto reply = server.handleJson (R"({ "jsonrpc": "2.0", "id": 9, "method": "tools/call", "params": { "name": "echo", "arguments": { "text": "t" } } })");
                text = reply.message["result"]["content"][0]["text"].toString();
                finished = true;
            });

            for (int i = 0; i < 100 && ! finished; ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (10);

            worker.join();
            expectEquals (text, juce::String ("echo: t"));
            expect (calledOnMessageThread);

            // after shutdown no tool runs any more
            server.shutdown();
            auto reply = server.handleJson (R"({ "jsonrpc": "2.0", "id": 10, "method": "tools/call", "params": { "name": "echo", "arguments": {} } })");
            expect ((bool) reply.message["result"]["isError"]);
        }
    }

    void tools()
    {
        beginTest ("Notes and values: names, numbers, choices");
        {
            using T = sso::SynthTools;
            expectEquals (T::parseNote ("C4"), 60);
            expectEquals (T::parseNote ("A4"), 69);
            expectEquals (T::parseNote ("f#3"), 54);
            expectEquals (T::parseNote ("Bb2"), 46);
            expectEquals (T::parseNote ("C-1"), 0);
            expectEquals (T::parseNote (61), 61);
            expectEquals (T::parseNote ("61"), 61);
            expectEquals (T::parseNote ("H2"), -1);
            expectEquals (T::parseNote (200), -1);
            expectEquals (T::parseNote (60.5), -1);

            sso::ParamSpec choice;
            choice.kind = sso::ParamSpec::Kind::choice;
            choice.choices = { "Square", "Saw" };
            choice.minValue = 0; choice.maxValue = 1;
            expect (T::parseValue (choice, "saw") == 1);
            expect (T::parseValue (choice, 0) == 0);
            expect (! T::parseValue (choice, "Sine").has_value());
            expect (! T::parseValue (choice, 2).has_value());
            expectEquals (T::rangeText (choice), juce::String ("Square, Saw"));
        }

        beginTest ("Every synth describes itself and every parameter");
        {
            sso::hawk800::Hawk800Device hawk;
            sso::kiwi3p::Kiwi3PDevice kiwi3p;
            sso::kiwisix::KiwiSixDevice kiwisix;

            for (const sso::DeviceDefinition* d : { (const sso::DeviceDefinition*) &hawk, (const sso::DeviceDefinition*) &kiwi3p,
                                                    (const sso::DeviceDefinition*) &kiwisix })
            {
                auto summary = d->getSynthSummary();
                expect (summary.startsWith ("VOICES ") && summary.contains ("\nFLOW "), d->getDisplayName());

                for (auto& line : juce::StringArray::fromLines (summary))
                    expect (line.upToFirstOccurrenceOf (" ", false, false).containsOnly ("ABCDEFGHIJKLMNOPQRSTUVWXYZ"),
                            "summary lines start with a KEY: " + line);

                for (auto& spec : d->getParams())
                {
                    expect (spec.help.isNotEmpty(), d->getDisplayName() + ": no help for " + spec.id);
                    expect (spec.help.length() <= 140, d->getDisplayName() + ": help too long for " + spec.id);
                    expect (! spec.help.contains ("|") && ! spec.help.containsChar ('\n'), spec.id);
                }
            }
        }

        sso::hawk800::Hawk800Device device;
        McpDummyProcessor processor;
        juce::AudioProcessorValueTreeState state (processor, nullptr, "test", sso::SynthController::createParameterLayout (device));
        sso::SynthController controller (device, state);
        controller.getConnection().setOutput ("SSO-TEST-PORT-mcp-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()));
        controller.startUsing();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

        juce::TemporaryFile folder;
        folder.getFile().createDirectory();
        const juce::ScopeGuard removeFolder { [&] { folder.getFile().deleteRecursively(); } };
        sso::PresetLibrary library (device, folder.getFile());
        auto generator = sso::hawk800::createGenerator (device);

        sso::SynthTools synthTools ({ "SSO Hawk-800", "1.0.0", controller, library, generator.get() });
        sso::McpServer server ("sso-hawk800", "SSO Hawk-800", "1.0.0", synthTools.instructions());
        synthTools.addTo (server);

        auto call = [&] (const juce::String& tool, const juce::String& arguments)
        {
            auto reply = server.handleJson (R"({ "jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": { "name": ")" + tool
                                            + R"(", "arguments": )" + arguments + " } }");
            return sso::McpServer::ToolResult { reply.message["result"]["content"][0]["text"].toString(),
                                                reply.message["result"]["structuredContent"],
                                                (bool) reply.message["result"]["isError"] };
        };

        beginTest ("Tools: synth_info, set_parameters, get_sound");
        {
            expectEquals ((int) server.getTools().size(), 12);

            auto info = call ("synth_info", "{}");
            expect (info.text.contains ("SYNTH Korg Poly-800"), info.text.substring (0, 200));
            expect (info.text.contains ("\ncutoff | VCF | 0..99 | "));
            expect (info.text.contains ("\ndco1_wave | DCO 1 | Square, Saw | "));
            expect (info.text.contains ("GENERATOR categories"));

            auto cutoff = device.indexOf ("cutoff"), wave = device.indexOf ("dco1_wave");
            auto set = call ("set_parameters", R"({ "values": { "cutoff": 60, "dco1_wave": "square", "bogus": 1, "resonance": 500 } })");
            expect (! set.isError, set.text);
            expectEquals (controller.getValue (cutoff), 60);
            expectEquals (controller.getValue (wave), 0);
            expect (set.text.contains ("cutoff=60") && set.text.contains ("bogus: unknown") && set.text.contains ("resonance: 500"), set.text);

            expect (call ("set_parameters", R"({ "values": { "bogus": 1 } })").isError, "nothing valid -> error");

            auto sound = call ("get_sound", R"({ "section": "VCF" })");
            expect (sound.text.contains ("[VCF] cutoff=60"), sound.text);
            expect (! sound.text.contains ("[DCO 1]"));
            expectEquals ((int) sound.structured["values"]["cutoff"], 60);
            expect (call ("get_sound", R"({ "section": "XYZ" })").isError);

            expect (! call ("set_sound_name", R"({ "name": "Glass Pad" })").isError);
            expectEquals (controller.getPatchName(), juce::String ("Glass Pad"));
        }

        beginTest ("Tools: library and generator");
        {
            auto saved = call ("save_preset", R"({ "name": "Glass Pad", "category": "Pads", "tags": ["ai", "soft"] })");
            expect (! saved.isError, saved.text);
            library.rescan();
            expectEquals ((int) library.getPresets().size(), 1);
            expectEquals (library.getPresets()[0].bank, juce::String (sso::SynthTools::aiBank));

            expect (call ("save_preset", R"({ "name": "glass pad" })").isError, "same name: refused");
            expect (! call ("save_preset", R"({ "name": "Glass Pad", "overwrite": true })").isError, "unless overwrite");
            library.rescan();
            expectEquals ((int) library.getPresets().size(), 1);
            expect (call ("save_preset", R"({ "name": "X", "category": "Noise" })").isError, "unknown category");

            auto list = call ("list_presets", R"({ "query": "glass" })");
            expect (list.text.contains ("AI Sounds/Glass Pad | Glass Pad | AI Sounds | Pads | ai,soft"), list.text);

            auto generated = call ("generate_sound", R"({ "category": "bass" })");
            expect (! generated.isError, generated.text);
            expectEquals (controller.getPresetRef().category, juce::String ("Bass"));
            expect (call ("generate_sound", R"({ "category": "Pizza" })").isError);
            expect (call ("generate_sound", R"({ "category": "Bass", "keep": ["Pizza"] })").isError);
            expect (call ("vary_sound", R"({ "amount": 2 })").isError);
            expect (! call ("vary_sound", R"({ "amount": 0.2, "keep": ["VCF"] })").isError);

            auto loaded = call ("load_preset", R"({ "id": "Glass Pad" })");
            expect (! loaded.isError, loaded.text);
            expectEquals (controller.getValue (device.indexOf ("cutoff")), 60);
            expectEquals (controller.getPatchName(), juce::String ("Glass Pad"));
            expect (call ("load_preset", R"({ "id": "nothing" })").isError);

            expect (! call ("init_sound", "{}").isError);
            expectEquals (controller.getPatchName(), juce::String ("Init"));
        }

        beginTest ("Tools: notes, and nothing changes the sound while tuning");
        {
            expect (call ("play_notes", R"({ "notes": [] })").isError);
            expect (call ("play_notes", R"({ "notes": [{ "note": "X9" }] })").isError);
            expect (call ("play_notes", R"({ "notes": [{ "note": 60, "duration_ms": 10000, "start_ms": 25000 }] })").isError, "over 30 s");

            auto played = call ("play_notes", R"({ "notes": [{ "note": "C4" }, { "note": "E4" }, { "note": "G4", "duration_ms": 1000 }] })");
            expect (! played.isError, played.text);
            expect (played.text.contains ("3 notes (2.0 s)"), played.text);
            expect (! call ("stop_sound", "{}").isError);

            controller.setTuning (true);
            expect (call ("set_parameters", R"({ "values": { "cutoff": 10 } })").isError);
            expect (call ("play_notes", R"({ "phrase": "test" })").isError);
            expect (! call ("get_sound", "{}").isError, "reading is fine");
            controller.setTuning (false);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            expect (! call ("set_parameters", R"({ "values": { "cutoff": 10 } })").isError);
        }

        server.shutdown();
    }
};

static McpTests mcpTests;

/** Shows what an AI reads: SSO_Tests --mcp-call hawk800|kiwi3p|kiwisix <tool> [<json arguments>]
    (dummy MIDI port and an empty temporary library - nothing reaches a synth). */
int runMcpCall (const juce::StringArray& args)
{
    auto i = args.indexOf ("--mcp-call");
    auto deviceId = args[i + 1];
    std::unique_ptr<sso::DeviceDefinition> device;
    std::unique_ptr<sso::PatchGenerator> generator;

    if (deviceId == "hawk800")      { auto d = std::make_unique<sso::hawk800::Hawk800Device>(); generator = sso::hawk800::createGenerator (*d); device = std::move (d); }
    else if (deviceId == "kiwisix") { auto d = std::make_unique<sso::kiwisix::KiwiSixDevice>(); generator = sso::kiwisix::createGenerator (*d); device = std::move (d); }
    else                            { auto d = std::make_unique<sso::kiwi3p::Kiwi3PDevice>(); generator = sso::kiwi3p::createGenerator (*d); device = std::move (d); }

    McpDummyProcessor processor;
    juce::AudioProcessorValueTreeState state (processor, nullptr, "test", sso::SynthController::createParameterLayout (*device));
    sso::SynthController controller (*device, state);
    controller.getConnection().setInput ({});
    controller.getConnection().setOutput ("SSO-TEST-PORT-mcp-call");
    controller.startUsing();

    juce::TemporaryFile folder;
    folder.getFile().createDirectory();
    sso::PresetLibrary library (*device, folder.getFile());
    sso::SynthTools tools ({ "SSO " + deviceId, "0.0.0", controller, library, generator.get() });
    sso::McpServer server ("sso", "SSO", "0.0.0", tools.instructions());
    tools.addTo (server);

    auto arguments = args[i + 3].isNotEmpty() ? args[i + 3] : juce::String ("{}");
    auto reply = server.handleJson (R"({ "jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": { "name": ")" + args[i + 2]
                                    + R"(", "arguments": )" + arguments + " } }");
    std::cout << reply.message["result"]["content"][0]["text"].toString() << std::endl;
    server.shutdown();
    folder.getFile().deleteRecursively();
    return 0;
}

/** A test MCP endpoint for manual checks with curl or an AI client, on a dummy MIDI port:
    SSO_Tests --mcp-serve hawk800|kiwi3p|kiwisix <port> <seconds> */
int runMcpServe (const juce::StringArray& args)
{
    auto i = args.indexOf ("--mcp-serve");
    auto deviceId = args[i + 1];
    std::unique_ptr<sso::DeviceDefinition> device;
    std::unique_ptr<sso::PatchGenerator> generator;

    if (deviceId == "hawk800")      { auto d = std::make_unique<sso::hawk800::Hawk800Device>(); generator = sso::hawk800::createGenerator (*d); device = std::move (d); }
    else if (deviceId == "kiwisix") { auto d = std::make_unique<sso::kiwisix::KiwiSixDevice>(); generator = sso::kiwisix::createGenerator (*d); device = std::move (d); }
    else                            { auto d = std::make_unique<sso::kiwi3p::Kiwi3PDevice>(); generator = sso::kiwi3p::createGenerator (*d); device = std::move (d); }

    McpDummyProcessor processor;
    juce::AudioProcessorValueTreeState state (processor, nullptr, "test", sso::SynthController::createParameterLayout (*device));
    sso::SynthController controller (*device, state);
    controller.getConnection().setInput ({});
    controller.getConnection().setOutput ("SSO-TEST-PORT-mcp-serve");
    controller.startUsing();

    juce::TemporaryFile folder;
    folder.getFile().createDirectory();
    sso::PresetLibrary library (*device, folder.getFile());
    sso::SynthTools tools ({ "SSO test " + deviceId, "0.0.0", controller, library, generator.get() });
    sso::McpServer server ("sso-" + deviceId, "SSO test " + deviceId, "0.0.0", tools.instructions());
    tools.addTo (server);
    sso::McpHttpServer http (server, args[i + 2].getIntValue());

    if (! http.start())
    {
        std::cout << "port busy" << std::endl;
        return 1;
    }

    std::cout << "serving " << http.getUrl() << std::endl;
    juce::MessageManager::getInstance()->runDispatchLoopUntil (juce::jmax (1, args[i + 3].getIntValue()) * 1000);
    server.shutdown();
    http.stop();
    folder.getFile().deleteRecursively();
    return 0;
}
