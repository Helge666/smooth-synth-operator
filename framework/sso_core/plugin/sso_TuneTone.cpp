namespace sso
{

void TuneTone::prepare (double sampleRate, Wave wave)
{
    sampleRate = sampleRate > 0 ? sampleRate : 48000.0;
    table.assign ((size_t) tableSize, 0.0f);

    // Fourier series up to 90% of Nyquist: saw 1/k for every k, square 1/k for odd k
    auto highest = (int) (0.45 * sampleRate / frequency);

    for (int k = 1; k <= highest; ++k)
    {
        if (wave == Wave::square && k % 2 == 0)
            continue;

        auto amplitude = 1.0 / k;

        for (int i = 0; i < tableSize; ++i)
            table[(size_t) i] += (float) (amplitude * std::sin (juce::MathConstants<double>::twoPi * k * i / tableSize));
    }

    double sumSquares = 0.0;

    for (auto v : table)
        sumSquares += (double) v * v;

    auto scale = 1.0 / std::sqrt (sumSquares / tableSize);

    for (auto& v : table)
        v = (float) (v * scale);

    phase = 0.0;
    increment = frequency * tableSize / sampleRate;
    gain = 0.0f;
    gainStep = (float) (1000.0 / (fadeMs * sampleRate));
}

void TuneTone::process (juce::AudioBuffer<float>& buffer, bool on, float levelDb) noexcept
{
    auto target = juce::Decibels::decibelsToGain (levelDb);

    if (gain <= 0.0f)
        level = target;

    if (table.empty() || (! on && gain <= 0.0f))
        return;

    auto numChannels = buffer.getNumChannels();
    auto levelStep = 1.0f - std::exp (-gainStep * 3.0f);     // ~5 ms to follow a level change

    for (int s = 0; s < buffer.getNumSamples(); ++s)
    {
        gain = on ? juce::jmin (1.0f, gain + gainStep) : juce::jmax (0.0f, gain - gainStep);
        level += (target - level) * levelStep;

        auto i = (int) phase;
        auto frac = (float) (phase - i);
        auto a = table[(size_t) i], b = table[(size_t) ((i + 1) % tableSize)];
        auto sample = gain * level * (a + frac * (b - a));

        phase += increment;

        if (phase >= tableSize)
            phase -= tableSize;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.addSample (ch, s, sample);
    }
}

} // namespace sso
