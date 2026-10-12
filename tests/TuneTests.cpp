#include <sso_hawk800/sso_hawk800.h>
#include <sso_kiwi3p/sso_kiwi3p.h>
#include <sso_kiwisix/sso_kiwisix.h>

namespace
{
    struct TuneDummyProcessor final : public juce::AudioProcessor
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

    bool same (const juce::MidiMessage& a, const juce::MidiMessage& b)
    {
        return a.getRawDataSize() == b.getRawDataSize() && std::memcmp (a.getRawData(), b.getRawData(), (size_t) a.getRawDataSize()) == 0;
    }
}

class TuneTests final : public juce::UnitTest
{
public:
    TuneTests() : juce::UnitTest ("Tuning by ear (TUNE)", "SSO") {}

    void runTest() override
    {
        beginTest ("Tune sounds: A as note 69, values in range, filter fully open");
        {
            sso::hawk800::Hawk800Device hawk;
            sso::kiwi3p::Kiwi3PDevice kiwi3p;
            sso::kiwisix::KiwiSixDevice kiwisix;

            auto check = [this] (const sso::DeviceDefinition& device, const char* cutoffId, int cutoff, sso::DeviceDefinition::TuneSound::Wave wave)
            {
                auto tune = device.getTuneSound();
                expect (tune.has_value(), device.getDisplayName());

                if (! tune.has_value())
                    return;

                expectEquals (tune->note, 69);
                expect (tune->wave == wave, device.getDisplayName() + " reference waveform");
                expectEquals (device.getPatchName (tune->patch), juce::String ("Tune"));
                expectEquals (device.getValue (tune->patch, device.indexOf (cutoffId)), cutoff, device.getDisplayName());

                for (int i = 0; i < (int) device.getParams().size(); ++i)
                {
                    auto& spec = device.getParams()[(size_t) i];
                    auto v = device.getValue (tune->patch, i);
                    expect (v >= spec.minValue && v <= spec.maxValue, device.getDisplayName() + " " + spec.id);
                }

                // the synth stores it unchanged
                expect (device.asStored (tune->patch) == tune->patch || device.storesPatchNames() == false);
            };

            check (hawk, "cutoff", 99, sso::DeviceDefinition::TuneSound::Wave::square);
            check (kiwi3p, "cutoff", 127, sso::DeviceDefinition::TuneSound::Wave::saw);
            check (kiwisix, "cutoff", sso::kiwisix::KiwiSixDevice::maxValue12, sso::DeviceDefinition::TuneSound::Wave::saw);

            auto t = hawk.getTuneSound();
            auto value = [&] (const char* id) { return hawk.getValue (t->patch, hawk.indexOf (id)); };
            expectEquals (value ("dco1_octave"), 0, "16' + 16' = concert pitch");
            expectEquals (value ("dco1_harmonics"), 0, "a single footage");
            expectEquals (value ("dco_mode"), 0, "DCO 2 off");
            expectEquals (value ("chorus"), 0);
            expectEquals (value ("resonance"), 0);
        }

        beginTest ("Reference tone: 440 Hz at the wanted level, faded in and out");
        {
            for (auto wave : { sso::TuneTone::Wave::saw, sso::TuneTone::Wave::square })
            {
                for (auto rate : { 44100.0, 48000.0, 96000.0 })
                {
                    sso::TuneTone tone;
                    tone.prepare (rate, wave);

                    auto total = (int) rate;
                    juce::AudioBuffer<float> out (2, total);
                    out.clear();

                    for (int pos = 0; pos < total; pos += 512)
                    {
                        auto n = juce::jmin (512, total - pos);
                        juce::AudioBuffer<float> block (out.getArrayOfWritePointers(), 2, pos, n);
                        tone.process (block, true, -12.0f);
                    }

                    auto* x = out.getReadPointer (0);
                    float startPeak = 0;
                    for (int i = 0; i < 8; ++i)
                        startPeak = juce::jmax (startPeak, std::abs (x[i]));
                    expect (startPeak < 0.01f, "fades in");

                    // last half second: level and pitch
                    auto from = total / 2;
                    double sum = 0;
                    int crossings = 0;

                    for (int i = from; i < total; ++i)
                    {
                        sum += (double) x[i] * x[i];

                        if (x[i - 1] >= 0.0f && x[i] < 0.0f)
                            ++crossings;
                    }

                    auto rms = std::sqrt (sum / (total - from));
                    expectWithinAbsoluteError (rms, 0.25, 0.01, "RMS level");
                    expectWithinAbsoluteError (crossings, 220, 1, "440 Hz");
                    expect (out.getReadPointer (1)[total - 1] == x[total - 1], "both channels");

                    // off: silent after the fade
                    juce::AudioBuffer<float> tail (2, (int) (rate * 0.05));
                    tail.clear();
                    tone.process (tail, false, -12.0f);
                    expect (tone.isSilent());
                    expectEquals (tail.getReadPointer (0)[tail.getNumSamples() - 1], 0.0f);
                }
            }
        }

        beginTest ("Controller: the synth's sound is read first and sent back afterwards");
        {
            sso::hawk800::Hawk800Device device;
            TuneDummyProcessor processor;
            juce::AudioProcessorValueTreeState state (processor, nullptr, "test", sso::SynthController::createParameterLayout (device));
            sso::SynthController controller (device, state);

            juce::CriticalSection lock;
            std::vector<juce::MidiMessage> sent;
            controller.getConnection().onMessageSent = [&] (const juce::MidiMessage& m) { const juce::ScopedLock sl (lock); sent.push_back (m); };
            controller.getConnection().setOutput ("SSO-TEST-PORT-tune-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()));
            controller.startUsing();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

            auto midi = controller.getMidiSettings();
            auto cutoff = device.indexOf ("cutoff");
            auto appPatch = device.createInitPatch();
            device.setValue (appPatch, cutoff, 50);
            controller.loadPatch (appPatch, false);

            auto synthPatch = device.createInitPatch();         // the synth plays something else
            device.setValue (synthPatch, cutoff, 30);

            auto takeSent = [&]
            {
                const juce::ScopedLock sl (lock);
                return std::exchange (sent, {});
            };

            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            takeSent();

            expect (controller.canTune());
            controller.setTuning (true);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

            expect (controller.isTuning() && ! controller.isTuneSounding(), "waits for the synth's sound");
            auto out = takeSent();
            expect (out.size() == 1 && same (out[0], device.createEditBufferRequest (midi)), "asks for the edit buffer");

            controller.getConnection().processIncoming (device.createEditBufferDump (midi, synthPatch));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
            expect (! controller.isTuneSounding(), "the note waits until the synth has taken the sound");
            juce::MessageManager::getInstance()->runDispatchLoopUntil (800);

            expect (controller.isTuneSounding());
            auto tune = *device.getTuneSound();
            out = takeSent();
            expect (out.size() == 3, juce::String ((int) out.size()));

            if (out.size() == 3)
            {
                expect (out[0].isAllSoundOff());
                expect (same (out[1], device.createEditBufferDump (midi, tune.patch)), "tune sound");
                expect (out[2].isNoteOn() && out[2].getNoteNumber() == 69);
            }

            expectEquals (device.getValue (controller.getCurrentPatch(), cutoff), 50, "the app's sound is unchanged");

            // the tune sound coming back from the synth is ignored; edits wait
            controller.getConnection().processIncoming (device.createEditBufferDump (midi, tune.patch));
            auto* p = state.getParameter ("cutoff");
            p->setValueNotifyingHost (p->convertTo0to1 (60.0f));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            expectEquals (device.getValue (controller.getCurrentPatch(), cutoff), 60);
            expect (takeSent().empty(), "nothing sent while tuning");

            controller.setTuning (false);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
            expect (! controller.isTuning() && ! controller.isTuneSounding());
            out = takeSent();
            expect (out.size() == 4, juce::String ((int) out.size()));

            if (out.size() == 4)
            {
                expect (out[0].isNoteOff() && out[0].getNoteNumber() == 69);
                expect (out[1].isAllSoundOff());
                expect (same (out[2], device.createEditBufferDump (midi, synthPatch)), "the synth's sound restored");
                auto edit = device.createParamMessages (midi, cutoff, [] (int) { return 60; });
                expect (same (out[3], edit[0]), "then the edit made meanwhile");
            }

            // no answer: the app's sound is restored
            takeSent();
            controller.setTuning (true);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (2300);
            expect (controller.isTuneSounding(), "starts after the timeout");
            controller.setTuning (false);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
            out = takeSent();
            expect (! out.empty() && same (out.back(), device.createEditBufferDump (midi, controller.getCurrentPatch())));
        }
    }
};

static TuneTests tuneTests;
