#pragma once

namespace sso
{

/** Describes one sound parameter of a hardware synth: how it is presented,
    where it lives in the synth's patch data, and how it is transmitted.

    Values are always plain ints in the range [minValue, maxValue]. For choice
    parameters the value is the index into `choices`.
*/
struct ParamSpec
{
    enum class Kind { continuous, bipolar, choice, toggle };

    juce::String id;          // stable identifier (automation, presets) - never rename
    juce::String name;        // display name
    juce::String section;     // GUI grouping
    juce::String help;        // what it does, in a few words (for AI control, see SynthTools)
    Kind kind = Kind::continuous;

    int minValue = 0;
    int maxValue = 127;
    int defaultValue = 0;
    int centreValue = 64;     // bipolar only: value shown as 0
    juce::StringArray choices;

    // Location in the patch data: `bits` wide field starting at bit `shift` of byte `offset`.
    int offset = -1;
    int shift = 0;
    int bits = 7;

    // Wide values (e.g. 12 bit): the field at `offset` holds the high bits, the 7-bit byte at
    // `loOffset` the low bits. value = (high << 7) | low.
    int loOffset = -1;
    bool isWide() const noexcept     { return loOffset >= 0; }

    /** Optional mapping value -> stored field value, for when the synth's internal
        order differs from the display order. Empty = stored value equals value. */
    std::vector<int> storedValues;
    bool storedInverted = false;  // stored = maxValue - value + minValue

    // Transmission. cc < 0 means "no CC, use the device's single-parameter message".
    int cc = -1;
    std::vector<int> ccValues;    // per value: CC value to send, -1 = not reachable by CC; empty = send the value

    std::function<juce::String (int)> format;   // optional display formatter

    int numValues() const noexcept   { return maxValue - minValue + 1; }
    bool isDiscrete() const noexcept { return kind == Kind::choice || kind == Kind::toggle; }

    int toStored (int value) const noexcept
    {
        value = juce::jlimit (minValue, maxValue, value);

        if (! storedValues.empty())
            return storedValues[(size_t) (value - minValue)];

        return storedInverted ? (maxValue - value + minValue) : value;
    }

    int fromStored (int stored) const noexcept
    {
        if (! storedValues.empty())
        {
            for (size_t i = 0; i < storedValues.size(); ++i)
                if (storedValues[i] == stored)
                    return minValue + (int) i;

            // The field holds a combination we don't list (e.g. several flag bits set):
            // pick the first entry whose bits are all present.
            for (size_t i = 0; i < storedValues.size(); ++i)
                if (storedValues[i] != 0 && (stored & storedValues[i]) == storedValues[i])
                    return minValue + (int) i;

            return minValue;
        }

        stored = juce::jlimit (minValue, maxValue, stored);
        return storedInverted ? (maxValue - stored + minValue) : stored;
    }

    /** CC value to transmit, or -1 if this value can only be sent another way. */
    int ccValueFor (int value) const noexcept
    {
        if (cc < 0)
            return -1;

        value = juce::jlimit (minValue, maxValue, value);

        if (! ccValues.empty())
            return ccValues[(size_t) (value - minValue)];

        return juce::jlimit (ccMin, ccMax, value) == value ? value : -1;
    }

    // Range the hardware accepts via CC for direct values (outside -> SysEx)
    int ccMin = 0, ccMax = 127;

    juce::String toText (int value) const
    {
        if (format != nullptr)
            return format (value);

        switch (kind)
        {
            case Kind::choice:  return choices[value - minValue];
            case Kind::toggle:  return value != 0 ? "On" : "Off";
            case Kind::bipolar: { auto v = value - centreValue; return (v > 0 ? "+" : "") + juce::String (v); }
            case Kind::continuous: break;
        }

        return juce::String (value);
    }
};

} // namespace sso
