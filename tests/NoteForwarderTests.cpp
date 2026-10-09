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

    /** Controller on a made-up port that records what would be sent. */
    struct Instance
    {
        Instance (const sso::DeviceDefinition& device, const juce::String& port)
            : state (processor, nullptr, "test", sso::SynthController::createParameterLayout (device)),
              controller (device, state)
        {
            controller.getConnection().setOutput (port);
            controller.getConnection().onMessageSent = [this] (const juce::MidiMessage& m)
            {
                const juce::ScopedLock sl (lock);
                sent.push_back ({ m, juce::Time::getMillisecondCounterHiRes() });
            };
            controller.startUsing();
        }

        std::vector<std::pair<juce::MidiMessage, double>> takeSent()
        {
            const juce::ScopedLock sl (lock);
            auto copy = sent;
            sent.clear();
            return copy;
        }

        DummyProcessor processor;
        juce::AudioProcessorValueTreeState state;
        sso::SynthController controller;
        juce::CriticalSection lock;
        std::vector<std::pair<juce::MidiMessage, double>> sent;
    };

    void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }
}

class NoteForwarderTests final : public juce::UnitTest
{
public:
    NoteForwarderTests() : juce::UnitTest ("Note forwarding", "SSO") {}

    void runTest() override
    {
        sso::kiwi3p::Kiwi3PDevice device;
        auto port = "SSO-TEST-PORT-notes-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt());
        Instance owner (device, port);
        pump (50);
        expect (owner.controller.isActive());

        sso::NoteForwarder forwarder (owner.controller);
        forwarder.prepare (48000.0, 480);        // 10 ms blocks
        owner.takeSent();

        beginTest ("Notes and performance controls go to the synth's channel, other CCs don't");
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (5, 60, (juce::uint8) 100), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (5, 74, 20), 1);      // would change a synth parameter
            midi.addEvent (juce::MidiMessage::controllerEvent (5, 1, 64), 2);       // mod wheel
            midi.addEvent (juce::MidiMessage::controllerEvent (5, 64, 127), 3);     // sustain
            midi.addEvent (juce::MidiMessage::pitchWheel (5, 9000), 4);
            midi.addEvent (juce::MidiMessage::channelPressureChange (5, 50), 5);
            midi.addEvent (juce::MidiMessage::programChange (5, 3), 6);
            midi.addEvent (juce::MidiMessage::noteOff (5, 60), 7);
            midi.addEvent (juce::MidiMessage::controllerEvent (5, 64, 0), 8);
            forwarder.process (midi, 480, true);
            pump (100);

            auto sent = owner.takeSent();
            expectEquals ((int) sent.size(), 7, "notes, mod wheel, sustain x2, pitch bend, pressure");

            for (auto& [m, t] : sent)
            {
                expectEquals (m.getChannel(), 1, "moved to the synth's channel");
                expect (! (m.isController() && m.getControllerNumber() == 74), "CC 74 must not pass");
                expect (! m.isProgramChange(), "program change must not pass");
            }

            expect (sent.size() > 1 && sent[0].first.isNoteOn() && sent.back().first.isSustainPedalOff(), "order kept");
        }

        beginTest ("Timing follows the position in the block plus the offset");
        {
            forwarder.setUserOffsetMs (20.0);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100), 24000);   // 500 ms into the block
            auto start = juce::Time::getMillisecondCounterHiRes();
            forwarder.process (midi, 48000, true);
            pump (700);

            auto sent = owner.takeSent();
            expectEquals ((int) sent.size(), 1);

            if (! sent.empty())
            {
                auto latency = sent[0].second - start;
                logMessage ("sent after " + juce::String (latency, 1) + " ms (expected 530)");
                expect (std::abs (latency - 530.0) < 6.0, "due time: 10 ms block + 500 ms position + 20 ms offset");
            }

            forwarder.setUserOffsetMs (0.0);
        }

        beginTest ("Held notes are released when the transport stops");
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 1);
            forwarder.process (midi, 480, true);
            pump (60);
            owner.takeSent();

            forwarder.process ({}, 480, false);     // stop
            pump (60);
            auto sent = owner.takeSent();
            auto releasedNote = std::any_of (sent.begin(), sent.end(), [] (auto& s) { return s.first.isNoteOff() && s.first.getNoteNumber() == 64; });
            auto releasedPedal = std::any_of (sent.begin(), sent.end(), [] (auto& s) { return s.first.isSustainPedalOff(); });
            expect (releasedNote && releasedPedal, "note-off and pedal up after stop");
        }

        beginTest ("An inactive instance forwards nothing");
        {
            Instance other (device, port);     // same synth: inactive
            pump (50);
            expect (! other.controller.isActive());

            sso::NoteForwarder otherForwarder (other.controller);
            otherForwarder.prepare (48000.0, 480);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 65, (juce::uint8) 100), 0);
            otherForwarder.process (midi, 480, true);
            pump (60);
            expectEquals ((int) other.takeSent().size(), 0);
        }

        beginTest ("Forwarding can be switched off");
        {
            forwarder.setEnabled (false);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 66, (juce::uint8) 100), 0);
            forwarder.process (midi, 480, true);
            pump (60);
            expectEquals ((int) owner.takeSent().size(), 0);
            forwarder.setEnabled (true);
        }
    }
};

static NoteForwarderTests noteForwarderTests;
