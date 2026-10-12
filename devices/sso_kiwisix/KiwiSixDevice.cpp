namespace sso::kiwisix
{

using Kind = ParamSpec::Kind;

namespace
{
    // CC values in the middle of the ranges the firmware decodes
    const std::vector<int> cc2 { 100, 16 };                     // 64-127 / 0-63 (VCA mode: EG / gate)
    const std::vector<int> cc3 { 16, 48, 96 };                  // 0-31 / 32-63 / 64-127
    const std::vector<int> ccWave { 8, 24, 40, 56, 96 };        // 16 per wave, 64-127 = random

    std::vector<int> ccSteps10 (int count)                      // clock dividers: 10 per step
    {
        std::vector<int> v;
        for (int i = 0; i < count; ++i)
            v.push_back (juce::jmin (127, i * 10 + 5));
        return v;
    }

    const juce::StringArray lfoWaves { "Triangle", "Square", "Saw", "Rev Saw", "Random" };
    const juce::StringArray clockDivides { "1/2", "1/4", "1/8", "1/8 half swing", "1/8 swing", "1/8 triplet",
                                           "1/16", "1/16 half swing", "1/16 swing", "1/16 triplet",
                                           "1/32", "1/32 triplet", "1/64 triplet" };

    juce::String percent (int v)   { return juce::String (juce::roundToInt (v * 100.0 / KiwiSixDevice::maxValue12)); }

    /** A 12-bit control: high 5 bits at `offset`, low 7 bits in the next byte. */
    ParamSpec wide (const char* id, const char* name, const char* section, int offset, int cc, int defaultValue)
    {
        ParamSpec s;
        s.id = id; s.name = name; s.section = section;
        s.kind = Kind::continuous;
        s.offset = offset; s.bits = 5; s.loOffset = offset + 1;
        s.minValue = 0; s.maxValue = KiwiSixDevice::maxValue12; s.defaultValue = defaultValue;
        s.cc = cc;
        s.format = percent;
        return s;
    }

    ParamSpec wideBipolar (const char* id, const char* name, const char* section, int offset, int cc)
    {
        auto s = wide (id, name, section, offset, cc, 2048);
        s.kind = Kind::bipolar;
        s.centreValue = 2048;
        s.format = [] (int v) { auto p = juce::roundToInt ((v - 2048) * 100.0 / 2047.0); return (p > 0 ? "+" : "") + juce::String (p); };
        return s;
    }

    ParamSpec choice (const char* id, const char* name, const char* section, int offset, int shift, int bits,
                      const juce::StringArray& choices, int cc, const std::vector<int>& ccValues,
                      int defaultValue = 0, std::vector<int> storedValues = {})
    {
        ParamSpec s;
        s.id = id; s.name = name; s.section = section;
        s.kind = Kind::choice;
        s.choices = choices;
        s.minValue = 0; s.maxValue = choices.size() - 1; s.defaultValue = defaultValue;
        s.offset = offset; s.shift = shift; s.bits = bits;
        s.cc = cc;
        s.ccValues = cc >= 0 ? ccValues : std::vector<int>();
        s.storedValues = std::move (storedValues);
        return s;
    }

    ParamSpec toggle (const char* id, const char* name, const char* section, int offset, int shift, int defaultValue = 0)
    {
        ParamSpec s;
        s.id = id; s.name = name; s.section = section;
        s.kind = Kind::toggle;
        s.minValue = 0; s.maxValue = 1; s.defaultValue = defaultValue;
        s.offset = offset; s.shift = shift; s.bits = 1;
        return s;   // no CC: sent with SysEx as the whole byte
    }

    constexpr int unknownByte = 0x42;       // the firmware stores 7 here when it is 0
}

KiwiSixDevice::KiwiSixDevice() : KiwiTechnicsDevice (0x02)    // F0 00 21 16 60 [02] = KiwiSix
{
    auto& p = params;

    // VCO --------------------------------------------------------------------------
    p.push_back (choice ("vco_octave", "Octave", "VCO", 0x14, 0, 2, { "16'", "8'", "4'" }, 8, cc3, 1));
    p.push_back (choice ("vco_wave", "Waveform", "VCO", 0x15, 0, 2, { "Saw", "PW", "PWM" }, 9, cc3));
    p.push_back (wide ("pw_pwm", "PW/PWM", "VCO", 0x16, 16, 0));
    p.push_back (wide ("pwm_speed", "PWM Speed", "VCO", 0x18, 33, 1024));
    p.push_back (choice ("pwm_wave", "PWM Wave", "VCO", 0x3f, 0, 3, lfoWaves, 66, ccWave));
    p.push_back (choice ("sub_osc", "Sub Osc", "VCO", 0x1a, 0, 2, { "Off", "1 Oct Down", "2 Oct Down" }, 34, cc3));

    // VCF --------------------------------------------------------------------------
    p.push_back (wide ("cutoff", "Cutoff", "VCF", 0x1c, 41, 3200));
    p.push_back (wide ("resonance", "Resonance", "VCF", 0x1e, 42, 0));
    p.push_back (wideBipolar ("eg_int", "EG Intensity", "VCF", 0x20, 47));
    p.back().defaultValue = 2600;
    p.push_back (wide ("kbd_track", "KBD Track", "VCF", 0x22, 43, 2048));

    // VCA --------------------------------------------------------------------------
    p.push_back (wide ("vca_level", "Level", "VCA", 0x25, 7, 1983));       // the factory sounds' level
    p.push_back (choice ("vca_mode", "Mode", "VCA", 0x27, 0, 1, { "EG", "Gate" }, 61, cc2));

    // EG ---------------------------------------------------------------------------
    p.push_back (wide ("eg_attack", "Attack", "EG", 0x28, 51, 0));
    p.push_back (wide ("eg_decay", "Decay", "EG", 0x2a, 52, 2048));
    p.push_back (wide ("eg_sustain", "Sustain", "EG", 0x2c, 53, 3200));
    p.push_back (wide ("eg_release", "Release", "EG", 0x2e, 54, 1200));

    // MG ---------------------------------------------------------------------------
    p.push_back (wide ("mg_rate", "Frequency", "MG", 0x30, 19, 2400));
    p.push_back (wide ("mg_delay", "Delay", "MG", 0x32, 20, 0));
    p.push_back (wide ("mg_level", "Level", "MG", 0x34, 49, 0));
    // Destinations are flags (VCO 1, VCF 2, VCA 4 - the manual has them reversed). CC 50 can
    // only select one, so they are sent with SysEx together with the wave in the same byte.
    p.push_back (toggle ("mg_vco", "To VCO", "MG", 0x36, 0, 1));
    p.push_back (toggle ("mg_vcf", "To VCF", "MG", 0x36, 1));
    p.push_back (toggle ("mg_vca", "To VCA", "MG", 0x36, 2));
    p.push_back (choice ("mg_wave", "Wave", "MG", 0x36, 3, 3, lfoWaves, 65, ccWave));

    // Effects ----------------------------------------------------------------------
    // Stored 1 chorus, 2 phase, 4 ensemble - verified with the panel switch (the manual's tone
    // table lists the flags in another order). CC 118 decodes quarters in the same order.
    p.push_back (choice ("fx_type", "Mode", "Effects", 0x39, 0, 3, { "Off", "Chorus", "Phase", "Ensemble" },
                         118, { 16, 48, 80, 112 }, 0, { 0, 1, 2, 4 }));
    p.push_back (wide ("fx_speed", "Speed/Intensity", "Effects", 0x37, 63, 2048));

    // Key assign and arpeggiator ---------------------------------------------------
    p.push_back (choice ("key_assign", "Key Assign", "Key Assign", 0x3a, 0, 2, { "Poly 1", "Poly 2", "Unison" }, 104, cc3));
    p.push_back (toggle ("hold", "Hold", "Key Assign", 0x3c, 0));
    // 0 = off (most factory sounds), 1-3 = the panel's 1 Oct / 2 Oct / Full. CC 109 only reaches 1-3.
    p.push_back (choice ("arp_range", "Range", "Arpeggiator", 0x3b, 0, 2, { "Off", "1 Oct", "2 Oct", "Full" }, 109, { -1, 16, 48, 96 }));
    p.push_back (choice ("arp_clock", "Clock", "Arpeggiator", 0x40, 0, 4, clockDivides, 110, ccSteps10 (clockDivides.size()), 6));
    p.push_back (choice ("seq_clock", "Seq Clock", "Arpeggiator", 0x41, 0, 4, clockDivides, 111, ccSteps10 (clockDivides.size()), 6));

    addHelp();
}

//==============================================================================
juce::String KiwiSixDevice::getSlotName (int slot) const
{
    // 8 banks A-H with 64 tones each, numbered 1-1 to 8-8 like the synth's two-button selection
    return getBankName (slot / 64) + juce::String ((slot % 64) / 8 + 1) + "-" + juce::String (slot % 8 + 1);
}

juce::String KiwiSixDevice::getBankName (int bank) const
{
    return juce::String::charToString ((juce::juce_wchar) ('A' + bank));
}

Patch KiwiSixDevice::createInitPatch() const
{
    return asStored (DeviceDefinition::createInitPatch());
}

std::optional<DeviceDefinition::TuneSound> KiwiSixDevice::getTuneSound() const
{
    // 8' sawtooth without sub oscillator, filter fully open, no modulation, gate, effects off
    auto p = createPatchWith ({ { "vco_octave", 1 }, { "vco_wave", 0 }, { "sub_osc", 0 },
                                { "cutoff", maxValue12 }, { "resonance", 0 }, { "eg_int", 2048 },
                                { "vca_mode", 1 }, { "mg_level", 0 }, { "mg_vco", 0 }, { "mg_vcf", 0 }, { "mg_vca", 0 },
                                { "fx_type", 0 }, { "key_assign", 0 }, { "hold", 0 }, { "arp_range", 0 } });
    setPatchName (p, "Tune");
    return TuneSound { asStored (p), 69, TuneSound::Wave::saw };
}

Patch KiwiSixDevice::asStored (const Patch& p) const
{
    auto stored = p;

    if (stored.data.size() > (size_t) unknownByte && stored.data[(size_t) unknownByte] == 0)
        stored.data[(size_t) unknownByte] = 7;

    return stored;
}

//==============================================================================
std::vector<juce::MidiMessage> KiwiSixDevice::createParamMessages (const MidiSettings& settings, int paramIndex,
                                                                   const std::function<int (int)>& valueOf) const
{
    auto& spec = params[(size_t) paramIndex];
    auto value = valueOf (paramIndex);

    // 12-bit controls: SysEx 0E offset, high 5 bits, low 7 bits (CC would only set the upper 7 bits)
    if (spec.isWide())
    {
        auto stored = spec.toStored (value);
        return { createSysEx (settings, editBufferParameter,
                              { (uint8_t) spec.offset, (uint8_t) ((stored >> 7) & 0x1f), (uint8_t) (stored & 0x7f) }) };
    }

    if (auto ccValue = spec.ccValueFor (value); ccValue >= 0)
        return { juce::MidiMessage::controllerEvent (settings.channel, spec.cc, ccValue) };

    auto byte = buildFieldByte (spec.offset, valueOf);
    return { createSysEx (settings, editBufferParameter, { (uint8_t) spec.offset, 0, (uint8_t) byte }) };
}

} // namespace sso::kiwisix
