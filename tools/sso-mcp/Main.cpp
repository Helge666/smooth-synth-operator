#include <sso_core/sso_core.h>

#include <iostream>
#include <string>

#if JUCE_WINDOWS
 #include <fcntl.h>
 #include <io.h>
#endif

/*  sso-mcp - lets AI programs that start MCP servers as programs (stdio, e.g. Claude Desktop)
    talk to an SSO app's AI control.

    sso-mcp kiwi3p | kiwisix | hawk800        (or: sso-mcp --port <n>)

    The app must run with AI control switched on (MIDI window). Messages are read line by line
    from stdin and answered on stdout; notes go to stderr. See docs/ai-control.md.
*/

namespace
{
    struct App
    {
        const char* id;
        const char* name;
        int port;
    };

    const App apps[] = { { "kiwi3p", "SSO Kiwi-3P", 47831 }, { "kiwisix", "SSO KiwiSix", 47832 }, { "hawk800", "SSO Hawk-800", 47833 } };

    int usage()
    {
        std::cerr << "usage: sso-mcp kiwi3p | kiwisix | hawk800   (or: sso-mcp --port <n>)" << std::endl;
        return 2;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;     // sockets and JSON only; no window
    juce::StringArray args (argv + 1, argc - 1);

   #if JUCE_WINDOWS
    // exact bytes: no CR added to the newline that ends each message
    _setmode (_fileno (stdin), _O_BINARY);
    _setmode (_fileno (stdout), _O_BINARY);
   #endif

    int port = 0;
    juce::String name;

    if (auto i = args.indexOf ("--port"); i >= 0)
    {
        port = args[i + 1].getIntValue();
        name = "The SSO app on port " + juce::String (port);
    }
    else
    {
        for (auto& a : apps)
            if (args[0].equalsIgnoreCase (a.id))
            {
                port = a.port;
                name = a.name;
            }
    }

    if (port <= 0)
        return usage();

    sso::McpBridge bridge (port, name);
    std::cerr << "sso-mcp: forwarding to " << bridge.getUrl() << std::endl;

    std::string line;

    while (std::getline (std::cin, line))
    {
        if (! line.empty() && line.back() == '\r')
            line.pop_back();

        auto reply = bridge.handleLine (juce::String::fromUTF8 (line.data(), (int) line.size()));

        if (reply.isNotEmpty())
        {
            std::cout << reply.toRawUTF8() << '\n';
            std::cout.flush();
        }
    }

    return 0;
}
