#include <sso_kiwi3p/sso_kiwi3p.h>

class MidiLoopTests final : public juce::UnitTest
{
public:
    MidiLoopTests() : juce::UnitTest ("MIDI loop protection", "SSO") {}

    void runTest() override
    {
        sso::kiwi3p::Kiwi3PDevice device;
        sso::DeviceDefinition::MidiSettings settings;

        beginTest ("Echoed messages stop sending");
        {
            sso::MidiConnection connection;     // no ports: send() only records, processIncoming() simulates the input
            int received = 0, loopCallbacks = 0;
            connection.onMessageReceived = [&] (const juce::MidiMessage&) { ++received; };
            connection.onLoopDetected = [&] { ++loopCallbacks; };

            auto cc = juce::MidiMessage::controllerEvent (1, 41, 99);
            auto request = device.createEditBufferRequest (settings);

            connection.send (cc);
            connection.processIncoming (cc);            // 1st echo
            expect (! connection.isLoopDetected(), "a single echo is not yet a loop");

            connection.send (request);
            connection.processIncoming (request);       // 2nd echo
            connection.send (cc);
            connection.processIncoming (cc);            // 3rd echo
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

            expect (connection.isLoopDetected(), "three echoes within 2 s are a loop");
            expectEquals (loopCallbacks, 1);
            expectEquals (received, 0, "echoes are not passed on");
            expectEquals ((int) connection.getNumSent(), 0, "nothing reaches a (closed) output anyway");

            connection.clearLoopDetected();
            expect (! connection.isLoopDetected());
        }

        beginTest ("Answers of the synth are not mistaken for echoes");
        {
            sso::MidiConnection connection;
            int received = 0;
            connection.onMessageReceived = [&] (const juce::MidiMessage&) { ++received; };

            // We send a dump, the synth later answers a request with the identical dump
            auto patch = device.createInitPatch();
            auto dump = device.createEditBufferDump (settings, patch);

            for (int i = 0; i < 5; ++i)
            {
                connection.send (dump);
                connection.processIncoming (dump);
            }

            // A CC we sent long ago and one that differs
            connection.send (juce::MidiMessage::controllerEvent (1, 41, 99));
            connection.processIncoming (juce::MidiMessage::controllerEvent (1, 41, 98));
            connection.processIncoming (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 64));

            expect (! connection.isLoopDetected(), "dumps and other messages do not trigger the protection");
            expectEquals (received, 7);
        }
    }
};

static MidiLoopTests midiLoopTests;
