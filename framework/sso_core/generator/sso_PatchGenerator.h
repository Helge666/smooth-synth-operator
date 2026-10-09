#pragma once

namespace sso
{

/** Rule-based random sound generator.

    A base rule set applies to every category; each category overrides the rules it
    needs. Parameters are grouped (DCO, VCF, ...) so groups can be kept unchanged
    ("masking"). Device-specific sanity checks run after every generation.
*/
class PatchGenerator
{
public:
    struct Rule
    {
        enum class Type { range, pick };

        Type type = Type::range;
        int low = 0, high = 0;
        std::vector<std::pair<int, float>> weighted;   // value, weight

        static Rule range (int lo, int hi)              { Rule r; r.low = lo; r.high = hi; return r; }
        static Rule fixed (int v)                       { return range (v, v); }
        static Rule pick (std::vector<std::pair<int, float>> w) { Rule r; r.type = Type::pick; r.weighted = std::move (w); return r; }

        int draw (juce::Random&) const;
    };

    using RuleSet = std::map<juce::String, Rule>;   // parameter id -> rule

    explicit PatchGenerator (const DeviceDefinition&);
    virtual ~PatchGenerator() = default;

    //==============================================================================
    void setBaseRules (RuleSet);
    void addCategory (const juce::String& name, RuleSet overrides, juce::StringArray nameWords);
    void assignGroup (const juce::String& section, const juce::String& group);

    juce::StringArray getCategories() const;
    juce::StringArray getGroups() const   { return groups; }
    juce::String getGroupOf (int paramIndex) const;

    //==============================================================================
    /** A new sound of `category`. Parameters in `keepGroups` are taken from `current`. */
    Patch generate (const juce::String& category, const Patch& current, const juce::StringArray& keepGroups,
                    juce::Random&) const;

    /** `current`, slightly changed. amount 0..1. */
    Patch vary (const Patch& current, float amount, const juce::StringArray& keepGroups, juce::Random&) const;

    /** Called after generate/vary to keep results playable. */
    std::function<void (Patch&, juce::Random&)> sanitise;

protected:
    juce::String makeName (const juce::String& category, juce::Random&) const;

    const DeviceDefinition& device;
    RuleSet baseRules;
    std::vector<std::tuple<juce::String, RuleSet, juce::StringArray>> categories;
    std::map<juce::String, juce::String> groupOfSection;
    juce::StringArray groups;
};

} // namespace sso
