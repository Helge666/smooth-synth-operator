#pragma once

namespace sso
{

/** Raw sound data as the hardware stores it. The device definition knows how to
    read and write parameters and the name inside `data`. Bytes the device does not
    describe are carried along untouched. */
struct Patch
{
    std::vector<uint8_t> data;

    bool isValid() const noexcept { return ! data.empty(); }

    juce::String toBase64() const
    {
        return juce::Base64::toBase64 (data.data(), data.size());
    }

    static Patch fromBase64 (const juce::String& text)
    {
        juce::MemoryOutputStream out;
        Patch p;

        if (juce::Base64::convertFromBase64 (out, text))
        {
            auto* bytes = static_cast<const uint8_t*> (out.getData());
            p.data.assign (bytes, bytes + out.getDataSize());
        }

        return p;
    }

    bool operator== (const Patch& other) const noexcept { return data == other.data; }
};

} // namespace sso
