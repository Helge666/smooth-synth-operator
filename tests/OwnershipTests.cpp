#include <sso_kiwi3p/sso_kiwi3p.h>

namespace
{
    struct DummyProcessor final : public juce::AudioProcessor
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

    /** A controller talking to a made-up MIDI port, so tests never touch the real synth
        (the port is remembered but cannot be opened; nothing is persisted). */
    struct TestInstance
    {
        TestInstance (const sso::DeviceDefinition& device, const juce::String& fakePort, const juce::String& name)
            : state (processor, nullptr, "test", sso::SynthController::createParameterLayout (device)),
              controller (device, state)
        {
            controller.getConnection().setOutput (fakePort);
            controller.setInstanceDescription (name);
        }

        DummyProcessor processor;
        juce::AudioProcessorValueTreeState state;
        sso::SynthController controller;
    };

    void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

    juce::File readyMarker (const juce::String& port)
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (port + ".ready");
    }

    juce::String fakePort (const char* tag)
    {
        return juce::String ("SSO-TEST-PORT-") + tag + "-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt());
    }
}

/** Child process for the cross-process test: holds the synth until another process takes over. */
int runHoldSynth (const juce::String& port)
{
    sso::kiwi3p::Kiwi3PDevice device;
    TestInstance instance (device, port, "test child process");
    pump (100);

    std::cout << (instance.controller.isActive() ? "OWNER" : "NOT-OWNER") << std::endl;
    readyMarker (port).replaceWithText ("ready");

    for (int i = 0; i < 100 && instance.controller.isActive(); ++i)
        pump (100);

    std::cout << (instance.controller.isActive() ? "STILL-OWNER" : "RELEASED") << std::endl;
    return 0;
}

class OwnershipTests final : public juce::UnitTest
{
public:
    OwnershipTests() : juce::UnitTest ("Synth ownership", "SSO") {}

    void runTest() override
    {
        sso::kiwi3p::Kiwi3PDevice device;

        beginTest ("Two instances in one process");
        {
            auto port = fakePort ("a");
            auto first = std::make_unique<TestInstance> (device, port, "first");
            auto second = std::make_unique<TestInstance> (device, port, "second");
            pump (50);

            expect (first->controller.isActive(), "first instance controls the synth");
            expect (! second->controller.isActive(), "second instance is inactive");
            expectEquals (second->controller.getOwnerDescription(), juce::String ("first"));

            second->controller.takeOver();
            pump (50);
            expect (second->controller.isActive() && ! first->controller.isActive(), "take over switches");
            expectEquals (first->controller.getOwnerDescription(), juce::String ("second"));

            // the owner goes away: the synth is free, the other instance waits for the user
            second.reset();
            pump (50);
            expect (! first->controller.isActive());
            expectEquals (first->controller.getOwnerDescription(), juce::String());

            first->controller.takeOver();
            pump (50);
            expect (first->controller.isActive(), "free synth taken over directly");

            // another synth (other port) is independent
            TestInstance other (device, fakePort ("b"), "other");
            pump (50);
            expect (other.controller.isActive(), "instance for another synth is active");

            // changing the port moves an instance to the other synth
            TestInstance mover (device, port, "mover");
            pump (50);
            expect (! mover.controller.isActive());
            mover.controller.getConnection().setOutput (fakePort ("c"));
            pump (50);
            expect (mover.controller.isActive(), "instance on a new port becomes active");
        }

        beginTest ("Instances in different processes");
        {
            auto port = fakePort ("x");
            auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
            juce::ChildProcess child;
            expect (child.start (juce::StringArray { exe.getFullPathName(), "--hold-synth", port }));

            // wait until the child has grabbed the synth
            for (int i = 0; i < 100 && ! readyMarker (port).existsAsFile(); ++i)
                pump (100);

            readyMarker (port).deleteFile();
            auto local = std::make_unique<TestInstance> (device, port, "parent");
            pump (50);

            expect (! local->controller.isActive(), "child process owns the synth");

            if (local != nullptr)
            {
                expectEquals (local->controller.getOwnerDescription(), juce::String ("test child process"));

                local->controller.takeOver();

                for (int i = 0; i < 40 && ! local->controller.isActive(); ++i)
                    pump (100);

                expect (local->controller.isActive(), "took over from the other process");
            }

            child.waitForProcessToFinish (15000);
            auto output = child.readAllProcessOutput();
            expect (output.contains ("OWNER") && output.contains ("RELEASED"), "child output: " + output);
        }
    }
};

static OwnershipTests ownershipTests;
