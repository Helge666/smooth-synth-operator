#pragma once

namespace sso
{

/** The app's reference for tuning by ear: the synth's tune waveform (sawtooth or square) at
    440 Hz. Built from the harmonics below Nyquist only, so it sounds clean at every sample
    rate, and faded in and out without clicks. Its level is the user's (TUNE overlay): how loud
    the synth arrives depends on its volume, the mixer and the interface.
*/
class TuneTone
{
public:
    using Wave = DeviceDefinition::TuneSound::Wave;

    static constexpr double frequency = 440.0;
    static constexpr double fadeMs = 15.0;

    /** Not while process() runs (prepareToPlay). */
    void prepare (double sampleRate, Wave);

    /** Adds the tone at `levelDb` (dBFS RMS) to every channel while `on`, fading in and out
        (also between levels). Audio thread. */
    void process (juce::AudioBuffer<float>&, bool on, float levelDb) noexcept;

    bool isSilent() const noexcept      { return gain <= 0.0f; }

private:
    static constexpr int tableSize = 4096;
    std::vector<float> table;           // one cycle, RMS 1
    double phase = 0.0, increment = 0.0;
    float gain = 0.0f, gainStep = 0.0f;   // gain: 0..1 fade
    float level = 0.0f;                   // linear, follows the wanted level smoothly
};

} // namespace sso
