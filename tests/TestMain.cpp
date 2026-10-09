#include <sso_kiwi3p/sso_kiwi3p.h>

int runHardwareTest (const juce::String& portName);
int runSnapshot (const juce::StringArray& args);
int runPlayTest (const juce::String& portName);
int runHoldSynth (const juce::String& port);
int runVst3StateTest (const juce::StringArray& args);

struct ConsoleLogger final : public juce::Logger
{
    void logMessage (const juce::String& message) override { std::cout << message << std::endl; }
};

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ConsoleLogger logger;
    juce::Logger::setCurrentLogger (&logger);
    const juce::ScopeGuard resetLogger { [] { juce::Logger::setCurrentLogger (nullptr); } };
    juce::StringArray args (argv + 1, argc - 1);

    if (args.contains ("--vst3-state"))
        return runVst3StateTest (args);

    if (args.contains ("--hold-synth"))
        return runHoldSynth (args[args.indexOf ("--hold-synth") + 1]);

    if (args.contains ("--play-test"))
        return runPlayTest (args[args.indexOf ("--play-test") + 1]);

    if (args.contains ("--snapshot"))
        return runSnapshot (args);

    if (args.contains ("--hardware"))
        return runHardwareTest (args[args.indexOf ("--hardware") + 1]);

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("SSO");

    int failures = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED: " + std::to_string (failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
