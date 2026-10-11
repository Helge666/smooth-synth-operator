namespace sso::kiwi3p
{

using Kind = ParamSpec::Kind;

namespace
{
    // CC values in the middle of the ranges the firmware decodes for 2/3/4/6/7-way switches
    const std::vector<int> cc2  { 0, 127 };
    const std::vector<int> cc3  { 16, 48, 96 };
    const std::vector<int> cc4  { 16, 48, 80, 112 };
    const std::vector<int> cc6  { 8, 24, 40, 56, 72, 100 };

    const juce::StringArray offDco { "Off", "DCO 1", "DCO 2", "Both" };
    const juce::StringArray offLfo { "Off", "LFO 1", "LFO 2", "Both" };

    ParamSpec continuous (const char* id, const char* name, const char* section, int offset, int cc,
                          int defaultValue, int minValue = 0, int maxValue = 127)
    {
        ParamSpec s;
        s.id = id; s.name = name; s.section = section;
        s.kind = Kind::continuous;
        s.offset = offset; s.cc = cc;
        s.minValue = minValue; s.maxValue = maxValue; s.defaultValue = defaultValue;
        return s;
    }

    ParamSpec bipolar (const char* id, const char* name, const char* section, int offset, int cc)
    {
        auto s = continuous (id, name, section, offset, cc, 63);
        s.kind = Kind::bipolar;
        s.centreValue = 63;
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

    ParamSpec toggle (const char* id, const char* name, const char* section, int offset, int shift, int cc = -1)
    {
        ParamSpec s;
        s.id = id; s.name = name; s.section = section;
        s.kind = Kind::toggle;
        s.minValue = 0; s.maxValue = 1; s.defaultValue = 0;
        s.offset = offset; s.shift = shift; s.bits = 1;
        s.cc = cc;
        if (cc >= 0) s.ccValues = cc2;
        return s;
    }

    juce::String semitones (int v)    { auto st = (v - 63) * 12.0 / 63.0; return (st > 0 ? "+" : "") + juce::String (st, 1) + " st"; }
    juce::String fineCents (int v)    { auto c = juce::roundToInt ((v - 63) * 50.0 / 63.0); return (c > 0 ? "+" : "") + juce::String (c) + " ct"; }
    juce::String mixBalance (int v)   { auto b = juce::roundToInt (v * 100.0 / 127.0); return juce::String (100 - b) + ":" + juce::String (b); }

    // Adds the 13 routing switches + level shared by mod wheel (base 0x45) and aftertouch (base 0x4b).
    void addModRouting (std::vector<ParamSpec>& p, const char* prefix, const char* section, int base, int levelOffset, int levelCc)
    {
        auto id = [prefix] (const char* suffix) { return juce::String (prefix) + suffix; };
        auto add = [&p] (ParamSpec s, const juce::String& fullId) { s.id = fullId; p.push_back (std::move (s)); };

        add (bipolar ("", "Depth", section, levelOffset, levelCc), id ("_depth"));
        add (choice ("", "DCO Pitch", section, base,     0, 2, offDco, -1, {}), id ("_dco_pitch"));
        add (choice ("", "DCO LFO",   section, base,     2, 2, offDco, -1, {}), id ("_dco_lfo"));
        add (choice ("", "DCO Env",   section, base,     4, 2, offDco, -1, {}), id ("_dco_env"));
        add (toggle ("", "DCO Mix",   section, base,     6),                    id ("_mix"));
        add (toggle ("", "VCF LFO",   section, base + 1, 0),                    id ("_vcf_lfo"));
        add (toggle ("", "VCF Env",   section, base + 1, 1),                    id ("_vcf_env"));
        add (toggle ("", "Cutoff",    section, base + 1, 2),                    id ("_cutoff"));
        add (toggle ("", "Resonance", section, base + 1, 3),                    id ("_reso"));
        add (toggle ("", "HPF",       section, base + 1, 4),                    id ("_hpf"));
        add (toggle ("", "VCA Level", section, base + 2, 0),                    id ("_vca_level"));
        add (toggle ("", "VCA LFO",   section, base + 2, 1),                    id ("_vca_lfo"));
        add (choice ("", "LFO Rate",  section, base + 3, 0, 2, offLfo, -1, {}), id ("_lfo_rate"));
        add (toggle ("", "Chorus",    section, base + 4, 0),                    id ("_chorus"));
    }
}

Kiwi3PDevice::Kiwi3PDevice() : KiwiTechnicsDevice (0x01)    // F0 00 21 16 60 [01] = 3P
{
    auto& p = params;

    // DCO 1 ------------------------------------------------------------------------
    p.push_back (choice ("dco1_range", "Range", "DCO 1", 0x14, 0, 2, { "16'", "8'", "4'" }, 8, cc3, 1));
    p.push_back (choice ("dco1_wave", "Wave", "DCO 1", 0x14, 2, 2, { "Saw", "Pulse", "Square" }, 9, cc3));
    p.push_back (bipolar ("dco1_tune", "Tune", "DCO 1", 0x15, 10));
    p.back().format = semitones;
    p.back().ccMax = 126;      // CC 127 is stored as 126, factory sounds use 127
    p.push_back (continuous ("dco1_lfo", "LFO Mod", "DCO 1", 0x17, 12, 0));
    p.push_back (choice ("dco1_lfo_src", "LFO Src", "DCO 1", 0x21, 1, 1, { "LFO 1", "LFO 2" }, 13, cc2));
    p.push_back (bipolar ("dco1_env", "Env Mod", "DCO 1", 0x16, 14));
    p.push_back (choice ("dco1_env_src", "Env Src", "DCO 1", 0x21, 0, 1, { "ENV 1", "ENV 2" }, 15, cc2));

    // DCO 2 ------------------------------------------------------------------------
    p.push_back (choice ("dco2_range", "Range", "DCO 2", 0x19, 0, 2, { "16'", "8'", "4'" }, 24, cc3, 1));
    p.push_back (choice ("dco2_wave", "Wave", "DCO 2", 0x19, 2, 2, { "Saw", "Pulse", "Square", "Noise" }, 25, cc4));
    p.push_back (bipolar ("dco2_tune", "Tune", "DCO 2", 0x1a, 26));
    p.back().format = semitones;
    p.back().ccMax = 126;
    p.push_back (bipolar ("dco2_fine", "Fine", "DCO 2", 0x1b, 27));
    p.back().format = fineCents;
    p.back().ccMax = 126;
    p.push_back (continuous ("dco2_lfo", "LFO Mod", "DCO 2", 0x1d, 28, 0));
    p.push_back (choice ("dco2_lfo_src", "LFO Src", "DCO 2", 0x21, 3, 1, { "LFO 1", "LFO 2" }, 29, cc2));
    p.push_back (bipolar ("dco2_env", "Env Mod", "DCO 2", 0x1c, 30));
    p.push_back (choice ("dco2_env_src", "Env Src", "DCO 2", 0x21, 2, 1, { "ENV 1", "ENV 2" }, 31, cc2));
    p.push_back (choice ("xmod", "X-Mod", "DCO 2", 0x1f, 0, 2, { "Off", "Sync", "Metal" }, 11, cc3));

    // Mixer ------------------------------------------------------------------------
    // CC 17: 0 = DCO 1 only, 127 = DCO 2 only. The patch stores it inverted.
    p.push_back (continuous ("dco_mix", "DCO 1:2", "Mixer", 0x20, 17, 64));
    p.back().storedInverted = true;
    p.back().format = mixBalance;
    p.push_back (choice ("env_velocity", "Env Velo", "Mixer", 0x21, 4, 2, offDco, -1, {}));

    // VCF --------------------------------------------------------------------------
    p.push_back (continuous ("hpf", "HPF", "VCF", 0x22, 44, 0));
    p.push_back (continuous ("cutoff", "Cutoff", "VCF", 0x23, 41, 127));
    p.push_back (continuous ("resonance", "Resonance", "VCF", 0x24, 42, 0));
    p.push_back (continuous ("vcf_lfo", "LFO Mod", "VCF", 0x25, 45, 0));
    p.push_back (choice ("vcf_lfo_src", "LFO Src", "VCF", 0x2a, 1, 1, { "LFO 1", "LFO 2" }, 46, cc2));
    p.push_back (bipolar ("vcf_env", "Env Mod", "VCF", 0x26, 47));
    p.push_back (choice ("vcf_env_src", "Env Src", "VCF", 0x2a, 0, 1, { "ENV 1", "ENV 2" }, 48, cc2));
    p.push_back (continuous ("key_follow", "Key Follow", "VCF", 0x27, 43, 64));

    // VCA --------------------------------------------------------------------------
    // Stored as flags: bit0 = gate (overrides), bit1 = ENV 2.
    p.push_back (choice ("vca_mode", "Mode", "VCA", 0x3e, 0, 2, { "Gate", "ENV 1", "ENV 2" }, 61, cc3, 2, { 1, 0, 2 }));
    p.push_back (continuous ("vca_level", "Level", "VCA", 0x3c, 7, 62));      // init as loud as the factory sounds
    p.push_back (continuous ("vca_lfo", "LFO Mod", "VCA", 0x3d, 59, 0));
    p.push_back (choice ("vca_lfo_src", "LFO Src", "VCA", 0x3e, 2, 1, { "LFO 1", "LFO 2" }, 60, cc2));

    // Envelopes --------------------------------------------------------------------
    p.push_back (continuous ("env1_attack", "Attack", "ENV 1", 0x2b, 51, 0));
    p.push_back (continuous ("env1_decay", "Decay", "ENV 1", 0x2c, 52, 64));
    p.push_back (continuous ("env1_sustain", "Sustain", "ENV 1", 0x2d, 53, 100));
    p.push_back (continuous ("env1_release", "Release", "ENV 1", 0x2e, 54, 30));
    p.push_back (continuous ("env2_attack", "Attack", "ENV 2", 0x2f, 65, 0));
    p.push_back (continuous ("env2_decay", "Decay", "ENV 2", 0x30, 66, 64));
    p.push_back (continuous ("env2_sustain", "Sustain", "ENV 2", 0x31, 67, 100));
    p.push_back (continuous ("env2_release", "Release", "ENV 2", 0x32, 68, 30));
    // CC 69 is acknowledged by the firmware but does not change the stored value -> SysEx.
    p.push_back (choice ("env_speed", "Speed 1+2", "ENV 2", 0x54, 0, 1, { "Normal", "Half" }, -1, {}));

    // LFOs -------------------------------------------------------------------------
    // Display order follows the CC table; the patch stores Square as 2.
    // Stored values 6/7 are undocumented for LFO 1 (and 7 for LFO 2) but occur in real
    // patches; they are kept selectable so such sounds survive editing (sent via SysEx).
    const juce::StringArray lfoWaves { "Sine", "Triangle", "Saw", "Rev Saw", "Square", "Random" };
    auto lfo1Waves = lfoWaves;
    lfo1Waves.addArray (juce::StringArray { "Wave 7", "Wave 8" });
    p.push_back (choice ("lfo1_wave", "Wave", "LFO 1", 0x33, 0, 3, lfo1Waves, 18,
                         { 8, 24, 40, 56, 72, 100, -1, -1 }, 1, { 0, 1, 3, 4, 2, 5, 6, 7 }));
    p.push_back (continuous ("lfo1_rate", "Rate", "LFO 1", 0x34, 19, 64));
    p.back().ccMin = 4;        // CC values below 4 are stored as 4, patches may hold 0-3
    p.push_back (continuous ("lfo1_delay", "Delay", "LFO 1", 0x35, 20, 0));
    p.push_back (choice ("lfo_mode", "Mode 1+2", "LFO 1", 0x39, 0, 2, { "Normal", "Plus" }, 16, { 16, 112 }));

    auto lfo2Waves = lfoWaves;
    lfo2Waves.addArray (juce::StringArray { "Random 2", "Wave 8" });
    p.push_back (choice ("lfo2_wave", "Wave", "LFO 2", 0x36, 0, 3, lfo2Waves, 21,
                         { 8, 24, 40, 56, 72, 88, 110, -1 }, 1, { 0, 1, 3, 4, 2, 5, 6, 7 }));
    p.push_back (continuous ("lfo2_rate", "Rate", "LFO 2", 0x37, 22, 64));
    p.back().ccMin = 4;
    p.push_back (continuous ("lfo2_delay", "Delay", "LFO 2", 0x38, 23, 0));

    // Chorus -----------------------------------------------------------------------
    // Stored as flags: bit0 = on, bit1 = auto.
    p.push_back (choice ("chorus", "Mode", "Chorus", 0x3b, 0, 2, { "Off", "Auto", "Manual" }, 57, cc3, 0, { 0, 3, 1 }));
    p.push_back (continuous ("chorus_rate", "Rate", "Chorus", 0x3a, 58, 47));

    // Voice ------------------------------------------------------------------------
    p.push_back (choice ("voice_mode", "Assign", "Voice", 0x52, 0, 3,
                         { "Poly 1", "Poly 2", "Unison 1", "Unison 2", "Mono 1", "Mono 2" }, 104, cc6));
    p.push_back (continuous ("detune", "Detune", "Voice", 0x53, 105, 0));
    p.push_back (continuous ("portamento", "Portamento", "Voice", 0x40, 5, 0));

    // Bender -----------------------------------------------------------------------
    p.push_back (choice ("bend_dco", "DCO Pitch", "Bender", 0x44, 0, 2, offDco, 35, cc4, 3));
    p.push_back (toggle ("bend_mix", "DCO Mix", "Bender", 0x44, 2, 39));
    p.push_back (toggle ("bend_cutoff", "Cutoff", "Bender", 0x44, 3, 36));
    p.push_back (choice ("bend_lfo", "LFO Rate", "Bender", 0x44, 4, 2, offLfo, 40, cc4));
    p.push_back (toggle ("bend_vca", "VCA Level", "Bender", 0x44, 6, 37));

    // Modulation (mod wheel and the LFO button) and aftertouch ---------------------
    addModRouting (p, "mw", "Mod Wheel", 0x45, 0x4a, 82);
    p.push_back (continuous ("lfo_button", "LFO Button", "Mod Wheel", 0x51, 83, 63));
    addModRouting (p, "at", "Aftertouch", 0x4b, 0x50, 103);

    // Sequencer links ----------------------------------------------------------------
    p.push_back (continuous ("load_seq", "Load Seq", "Sequencer", 0x42, 55, 0, 0, 8));
    p.back().format = [] (int v) { return v == 0 ? juce::String ("-") : "Seq " + juce::String (v); };
    p.push_back (continuous ("load_pattern", "Load Pattern", "Sequencer", 0x43, 56, 0, 0, 16));
    p.back().format = [] (int v) { return v == 0 ? juce::String ("-") : "Pat " + juce::String (v); };

    // Globals --------------------------------------------------------------------------
    // Musical settings kept outside the sounds (offsets in the 48-byte global dump). The
    // synth stores them only from its panel; the app can send them whenever it connects,
    // as the Kiwi Patch Editor did. Defaults are the values the Patch Editor used to set.
    // MIDI channels, device ID and MIDI enables (0x0a-0x16) are deliberately not included.
    auto& g = globalParams;
    const juce::StringArray clocks { "Internal", "LFO 2", "Pattern", "MIDI / 3", "MIDI / 4", "MIDI / 6", "MIDI / 9",
                                     "MIDI / 12", "MIDI / 18", "MIDI / 24", "MIDI / 36", "MIDI / 48", "MIDI / 72",
                                     "MIDI / 96", "MIDI / 128", "MIDI / 144", "MIDI / 168", "MIDI / 192" };
    auto patternClocks = clocks;
    patternClocks.remove (2);       // the pattern can't clock itself

    g.push_back (bipolar ("g_fine_tune", "Fine Tune", "Tuning", 0x17, -1));
    g.back().format = [] (int v) { auto c = juce::roundToInt ((v - 63) * 200.0 / 63.0); return (c > 0 ? "+" : "") + juce::String (c) + " ct"; };
    g.push_back (continuous ("g_velocity", "Internal Velocity", "Keyboard", 0x01, -1, 64));
    g.push_back (choice ("g_detune_mode", "Voice Detune", "Keyboard", 0x00, 0, 1, { "Unison/Mono", "All Modes" }, -1, {}));
    g.push_back (toggle ("g_arp_on", "Arpeggiator", "Arpeggiator", 0x02, 0));
    g.push_back (choice ("g_arp_direction", "Direction", "Arpeggiator", 0x02, 1, 2, { "Up", "Down", "Up & Down", "Random" }, -1, {}));
    g.push_back (choice ("g_arp_range", "Range", "Arpeggiator", 0x03, 0, 2, { "1 Oct", "2 Oct", "3 Oct" }, -1, {}));
    g.push_back (choice ("g_arp_clock", "Clock", "Arpeggiator", 0x04, 0, 5, clocks, -1, {}, 3));
    g.push_back (choice ("g_seq_local", "Plays", "Sequencer", 0x05, 0, 2, { "Local + MIDI", "Local Only", "MIDI Only" }, -1, {}));
    g.push_back (toggle ("g_seq_midi_start", "MIDI Start/Stop", "Sequencer", 0x05, 2));
    g.push_back (choice ("g_seq_clock", "Clock", "Sequencer", 0x06, 0, 5, clocks, -1, {}));
    g.push_back (toggle ("g_pattern_on", "Pattern", "Pattern", 0x08, 2));
    g.back().defaultValue = 1;
    g.push_back (toggle ("g_pattern_vcf", "To VCF", "Pattern", 0x08, 1));
    g.push_back (toggle ("g_pattern_vca", "To VCA", "Pattern", 0x08, 0));
    g.back().defaultValue = 1;
    g.push_back (continuous ("g_pattern_level", "Level", "Pattern", 0x07, -1, 50));
    g.push_back (choice ("g_pattern_clock", "Clock", "Pattern", 0x09, 0, 5, patternClocks, -1, {}, 2));
}

//==============================================================================
juce::String Kiwi3PDevice::getSlotName (int slot) const
{
    // 16 banks A-P with 16 tones each (sets 1-4 = A-D, E-H, I-L, M-P)
    return juce::String::charToString ((juce::juce_wchar) ('A' + slot / 16)) + "-" + juce::String (slot % 16 + 1);
}

Patch Kiwi3PDevice::createInitPatch() const
{
    auto p = DeviceDefinition::createInitPatch();

    // Unused bytes that the firmware keeps at the centre value in real patches
    for (auto offset : { 0x18, 0x1e, 0x28, 0x3f })
        p.data[(size_t) offset] = 0x3f;

    return p;
}

//==============================================================================
std::vector<juce::MidiMessage> Kiwi3PDevice::createParamMessages (const MidiSettings& settings, int paramIndex,
                                                                  const std::function<int (int)>& valueOf) const
{
    auto& spec = params[(size_t) paramIndex];

    if (auto ccValue = spec.ccValueFor (valueOf (paramIndex)); ccValue >= 0)
        return { juce::MidiMessage::controllerEvent (settings.channel, spec.cc, ccValue) };

    auto byte = buildFieldByte (spec.offset, valueOf);
    return { createSysEx (settings, editBufferParameter, { (uint8_t) spec.offset, (uint8_t) byte }) };
}

std::vector<int> Kiwi3PDevice::getParamsToRefreshAfterDump() const
{
    // Measured: after an edit-buffer dump the DCO footage switch can keep its previous
    // position until it is sent again.
    return { indexOf ("dco1_range"), indexOf ("dco1_wave"), indexOf ("dco2_range"), indexOf ("dco2_wave") };
}

} // namespace sso::kiwi3p
