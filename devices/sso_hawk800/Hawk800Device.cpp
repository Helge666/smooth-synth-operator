namespace sso::hawk800
{

using Kind = ParamSpec::Kind;

namespace
{
    // Korg, Poly-800
    constexpr uint8_t korg = 0x42, poly800 = 0x21;

    const juce::StringArray octaves { "16'", "8'", "4'" };
    const juce::StringArray waves { "Square", "Saw" };      // measured: 0 = equal footages, 1 = weighted (saw)

    // Harmonics selector: which footages of the octave sound (manual order)
    const juce::StringArray harmonics { "16'", "8'", "4'", "2'", "16'+8'", "16'+4'", "16'+2'", "8'+4'", "8'+2'", "4'+2'",
                                        "16'+8'+4'", "16'+8'+2'", "16'+4'+2'", "8'+4'+2'", "All", "Off" };

    const juce::StringArray lfoWaves { "Triangle", "Inv Triangle", "Saw", "Inv Saw", "Sine", "Inv Sine", "PWM Square",
                                       "Inv PWM Square", "S&H LFO", "S&H Clock", "S&H Seq Loop", "EG3", "Inv EG3" };
    const juce::StringArray lfoSources { "LFO1", "LFO2", "SLFO3", "SLFO4" };
    const juce::StringArray velocityTargets { "EG1 Attack", "EG1 Decay", "EG1 Sustain", "EG1 Release",
                                              "EG2 Attack", "EG2 Decay", "EG2 Sustain", "EG2 Release",
                                              "EG3 Attack", "EG3 Decay", "EG3 Sustain", "EG3 Release" };
    const juce::StringArray phases { "0 deg", "90 deg", "180 deg", "270 deg" };

    // byte offset of a parameter in the synth's numbering: P1 11-88 -> 0-63, P2 11-88 -> 64-127
    constexpr int p1 (int number) noexcept { return (number / 10 - 1) * 8 + number % 10 - 1; }
    constexpr int p2 (int number) noexcept { return 64 + p1 (number); }

    ParamSpec value (const char* id, const char* name, const char* section, int offset, int maxValue, int defaultValue,
                     int shownFrom = 0)
    {
        ParamSpec s;
        s.id = id; s.name = name; s.section = section;
        s.kind = Kind::continuous;
        s.offset = offset;
        s.minValue = 0; s.maxValue = maxValue; s.defaultValue = defaultValue;

        if (shownFrom != 0)
            s.format = [shownFrom] (int v) { return juce::String (v + shownFrom); };

        return s;
    }

    ParamSpec choice (const char* id, const char* name, const char* section, int offset,
                      const juce::StringArray& choices, int defaultValue = 0)
    {
        ParamSpec s;
        s.id = id; s.name = name; s.section = section;
        s.kind = Kind::choice;
        s.choices = choices;
        s.offset = offset;
        s.minValue = 0; s.maxValue = choices.size() - 1; s.defaultValue = defaultValue;
        return s;
    }

    ParamSpec toggle (const char* id, const char* name, const char* section, int offset, int defaultValue = 0)
    {
        ParamSpec s;
        s.id = id; s.name = name; s.section = section;
        s.kind = Kind::toggle;
        s.offset = offset;
        s.minValue = 0; s.maxValue = 1; s.defaultValue = defaultValue;
        return s;
    }

    juce::String interval (int semitones)
    {
        static const juce::StringArray names { "Unison", "Min 2nd", "Maj 2nd", "Min 3rd", "Maj 3rd", "4th", "Tritone",
                                               "5th", "Min 6th", "Maj 6th", "Min 7th", "Maj 7th", "Octave" };
        return names[semitones];
    }

    std::vector<uint8_t> fromNibbles (const uint8_t* data, int numNibbles)
    {
        std::vector<uint8_t> bytes;

        for (int i = 0; i + 1 < numNibbles; i += 2)
            bytes.push_back ((uint8_t) (((data[i] & 0x0f) << 4) | (data[i + 1] & 0x0f)));

        return bytes;
    }

    /** SysEx body after F0 if it is a Poly-800 message with this command, else nullptr. */
    const uint8_t* bodyOf (const juce::MidiMessage& m, uint8_t command, int minSize)
    {
        if (! m.isSysEx() || m.getSysExDataSize() < minSize)
            return nullptr;

        auto* d = m.getSysExData();
        return d[0] == korg && d[1] == poly800 && d[2] == command ? d : nullptr;
    }
}

Hawk800Device::Hawk800Device()
{
    auto& p = params;

    // DCO 1 and 2 -------------------------------------------------------------------
    auto dco = [&p] (const char* prefix, const char* section, int base)
    {
        auto id = [prefix] (const char* s) { return juce::String (prefix) + s; };
        auto add = [&p] (ParamSpec s, const juce::String& theId) { s.id = theId; p.push_back (std::move (s)); };

        add (choice ("", "Octave", section, p1 (base + 1), octaves, 1), id ("_octave"));
        add (choice ("", "Waveform", section, p1 (base + 2), waves, 1), id ("_wave"));
        add (choice ("", "Harmonics", section, p1 (base + 3), harmonics, 14), id ("_harmonics"));
        add (choice ("", "Harm Mod Wave", section, p1 (base + 4), lfoWaves), id ("_hm_wave"));
        add (choice ("", "Harm Mod Source", section, p1 (base + 5), lfoSources), id ("_hm_source"));
        add (value ("", "Harm Mod Depth", section, p1 (base + 6), 15, 0), id ("_hm_depth"));
        add (value ("", "Level", section, p1 (base + 7), 31, 31), id ("_level"));
    };

    dco ("dco1", "DCO 1", 10);
    p.push_back (choice ("dco_mode", "Mode", "DCO 1", p1 (18), { "Single", "Double" }));   // double = DCO 2 on, 4 voices
    dco ("dco2", "DCO 2", 20);
    p.push_back (value ("dco2_interval", "Interval", "DCO 2", p1 (31), 12, 0));            // factory sounds use 0-12
    p.back().format = interval;
    p.push_back (value ("dco2_detune", "Detune", "DCO 2", p1 (32), 3, 0));

    // Noise and chorus --------------------------------------------------------------
    p.push_back (value ("noise", "Noise Level", "Noise", p1 (33), 15, 0));
    p.push_back (toggle ("chorus", "Chorus", "Noise", p1 (48)));

    // VCF ---------------------------------------------------------------------------
    p.push_back (value ("cutoff", "Cutoff", "VCF", p1 (41), 99, 99));
    p.push_back (value ("resonance", "Resonance", "VCF", p2 (51), 99, 0));
    p.push_back (choice ("kbd_track", "Kbd Track", "VCF", p1 (42), { "Off", "Half", "Full" }));
    // Measured: stored 1 opens the filter (the manual has the two reversed)
    p.push_back (choice ("eg3_polarity", "EG3 Polarity", "VCF", p1 (43), { "-", "+" }, 1));
    p.push_back (value ("eg3_int", "EG3 Intensity", "VCF", p1 (44), 15, 0));

    // Envelopes: EG1 -> DCO 1 level, EG2 -> DCO 2 level, EG3 -> VCF (and modulation) -------
    auto eg = [&p] (const char* prefix, const char* section, int base, int release)
    {
        const char* names[] = { "Attack", "Decay", "Break Point", "Slope", "Sustain", "Release" };
        const char* ids[] = { "_attack", "_decay", "_break", "_slope", "_sustain", "_release" };
        const int defaults[] = { 0, 31, 31, 31, 31, release };

        for (int i = 0; i < 6; ++i)
        {
            auto s = value ("", names[i], section, p1 (base + 1 + i), 31, defaults[i]);
            s.id = juce::String (prefix) + ids[i];
            p.push_back (std::move (s));
        }
    };

    eg ("eg1", "EG 1", 50, 8);
    eg ("eg2", "EG 2", 60, 8);
    eg ("eg3", "EG 3", 70, 8);
    p.push_back (choice ("eg3_trigger", "EG3 Trigger", "EG 3", p1 (45), { "Single", "Multi" }));

    // Velocity ----------------------------------------------------------------------
    p.push_back (value ("vel1_int", "OP1 Intensity", "Velocity", p1 (81), 4, 0));
    p.push_back (toggle ("vel1_invert", "OP1 Invert", "Velocity", p1 (82)));
    p.push_back (choice ("vel1_target", "OP1 Target", "Velocity", p1 (83), velocityTargets));
    p.push_back (value ("vel2_int", "OP2 Intensity", "Velocity", p1 (84), 4, 0));
    p.push_back (toggle ("vel2_invert", "OP2 Invert", "Velocity", p1 (85)));
    p.push_back (choice ("vel2_target", "OP2 Target", "Velocity", p1 (86), velocityTargets));
    p.push_back (value ("vcf_vel_int", "VCF Intensity", "Velocity", p1 (87), 6, 0));
    p.push_back (choice ("vcf_vel_shape", "VCF Shape", "Velocity", p1 (88),
                         { "Linear", "Inv Linear", "S Shape", "Inv S Shape", "Log", "Inv Log" }));

    // LFOs --------------------------------------------------------------------------
    auto lfo = [&p] (const char* prefix, int base)
    {
        auto id = [prefix] (const char* s) { return juce::String (prefix) + s; };
        auto add = [&p] (ParamSpec s, const juce::String& theId) { s.id = theId; p.push_back (std::move (s)); };

        add (value ("", "Rate", "LFO", p2 (base + 1), 15, 7), id ("_rate"));
        add (value ("", "Delay", "LFO", p2 (base + 2), 15, 0), id ("_delay"));
        add (toggle ("", "Free Running", "LFO", p2 (base + 3)), id ("_free"));
        add (toggle ("", "Delay Invert", "LFO", p2 (base + 4)), id ("_delay_invert"));
        add (value ("", "PW Phase", "LFO", p2 (base + 5), 62, 0, 1), id ("_pw"));
        add (choice ("", "Rate Mod Wave", "LFO", p2 (base + 6), lfoWaves), id ("_fm_wave"));
        add (value ("", "Rate Mod Depth", "LFO", p2 (base + 7), 15, 0), id ("_fm_depth"));
    };

    lfo ("lfo1", 10);
    p.push_back (choice ("lfo1_phase", "Start Phase", "LFO", p2 (18), phases));
    lfo ("lfo2", 20);
    p.push_back (value ("lfo2_sync", "Clock Sync", "LFO", p2 (28), 63, 0));

    p.push_back (value ("slfo3_rate", "SLFO3 Rate", "LFO", p2 (71), 15, 0));
    p.push_back (value ("slfo3_pw", "SLFO3 PW Phase", "LFO", p2 (72), 62, 0, 1));
    p.push_back (toggle ("slfo3_free", "SLFO3 Free Running", "LFO", p2 (73)));
    p.push_back (value ("rsh_ticks", "S&H Clock Ticks", "LFO", p2 (74), 98, 0, 1));
    p.push_back (value ("slfo4_rate", "SLFO4 Rate", "LFO", p2 (75), 15, 0));
    p.push_back (value ("slfo4_pw", "SLFO4 PW Phase", "LFO", p2 (76), 62, 0, 1));
    p.push_back (toggle ("slfo4_free", "SLFO4 Free Running", "LFO", p2 (77)));
    p.push_back (choice ("slfo4_phase", "SLFO4 Start Phase", "LFO", p2 (78), phases));

    // Modulation routing ------------------------------------------------------------
    p.push_back (choice ("dco_lfo_wave", "DCO LFO Wave", "Modulation", p2 (31), lfoWaves));
    p.push_back (choice ("dco_lfo_source", "DCO LFO Source", "Modulation", p2 (32), lfoSources));
    p.push_back (value ("dco_lfo_depth", "DCO LFO Depth", "Modulation", p2 (33), 15, 0));
    p.push_back (choice ("dco_eg_invert", "DCO EG Mode", "Modulation", p2 (35), { "Add", "Subtract" }));
    p.push_back (value ("dco_eg_depth", "DCO EG Depth", "Modulation", p2 (36), 15, 0));
    p.push_back (choice ("dco_mod_mode", "DCO Mod Range", "Modulation", p2 (38), { "Normal", "Wide (no bend)", "Note biased" }));

    p.push_back (choice ("vcf_lfo1_wave", "VCF Mod 1 Wave", "Modulation", p2 (41), lfoWaves));
    p.push_back (choice ("vcf_lfo1_source", "VCF Mod 1 Source", "Modulation", p2 (42), lfoSources));
    p.push_back (value ("vcf_lfo1_depth", "VCF Mod 1 Depth", "Modulation", p2 (43), 15, 0));
    p.push_back (choice ("vcf_lfo2_wave", "VCF Mod 2 Wave", "Modulation", p2 (44), lfoWaves));
    p.push_back (choice ("vcf_lfo2_source", "VCF Mod 2 Source", "Modulation", p2 (45), lfoSources));
    p.push_back (value ("vcf_lfo2_depth", "VCF Mod 2 Depth", "Modulation", p2 (46), 15, 0));
    p.push_back (value ("vcf_mod2_mode", "VCF Mod 2 Attenuate", "Modulation", p2 (47), 15, 0));
    p.back().format = [] (int v) { return v == 0 ? juce::String ("Add") : juce::String (v); };

    p.push_back (choice ("reso_lfo_wave", "Reso LFO Wave", "Modulation", p2 (52), lfoWaves));
    p.push_back (choice ("reso_lfo_source", "Reso LFO Source", "Modulation", p2 (53), lfoSources));
    p.push_back (value ("reso_lfo_depth", "Reso LFO Depth", "Modulation", p2 (54), 15, 0));
    p.push_back (value ("reso_eg_depth", "Reso EG Depth", "Modulation", p2 (56), 15, 0));
    p.push_back (choice ("reso_eg_invert", "Reso EG Mode", "Modulation", p2 (57), { "Add", "Subtract" }));

    p.push_back (choice ("trem1_source", "Tremolo DCO1", "Modulation", p2 (81), { "Off", "LFO1", "LFO2" }));
    p.push_back (value ("trem1_depth", "Tremolo DCO1 Depth", "Modulation", p2 (82), 15, 0));
    p.push_back (choice ("trem2_source", "Tremolo DCO2", "Modulation", p2 (83), { "Off", "LFO1", "LFO2" }));
    p.push_back (value ("trem2_depth", "Tremolo DCO2 Depth", "Modulation", p2 (84), 15, 0));

    // Playing: key assign, portamento, bend, arpeggiator -----------------------------
    p.push_back (choice ("key_assign", "Key Assign", "Play", p2 (88), { "Poly", "Chord", "Hold", "Porta Mono", "Porta Unison" }));
    p.push_back (value ("portamento", "Portamento", "Play", p2 (86), 15, 0, 1));    // 1-8 fixed, 9-16 variable speed
    p.push_back (value ("bend_depth", "Bend Depth", "Play", p2 (85), 63, 0));
    p.push_back (value ("pedal_offset", "Pedal Decay/Sustain", "Play", p2 (87), 31, 0));
    p.push_back (value ("arp_length", "Arp Length", "Play", p1 (57), 15, 0, 1));
    p.push_back (choice ("arp_direction", "Arp Direction", "Play", p1 (58), { "Up", "Down", "Up/Down" }));
    p.push_back (choice ("arp_range", "Arp Range", "Play", p1 (67), { "1 Oct", "2 Oct", "3 Oct" }));
    p.push_back (choice ("arp_sort", "Arp Sort", "Play", p1 (68), { "None", "Down", "Up/Down" }));
    p.push_back (toggle ("arp_played", "Arp Sound Played", "Play", p1 (77)));

    // Not exposed: MK2 effects/EQ (P1 34-38, 46, 47) and AtomaHawk parameters (P2 48, 58, 61-68)
    // - their bytes are carried along unchanged.
}

//==============================================================================
juce::String Hawk800Device::getSlotName (int slot) const
{
    // Bank 1-4, patch 11-88 as on the synth's display
    auto inBank = slot % slotsPerBank;
    return juce::String (slot / slotsPerBank + 1) + "-" + juce::String (inBank / 8 + 1) + juce::String (inBank % 8 + 1);
}

Patch Hawk800Device::createInitPatch() const
{
    auto p = DeviceDefinition::createInitPatch();
    p.data[(size_t) p1 (46)] = 6;       // MK2 EQ flat, should the sound ever reach an MK2
    p.data[(size_t) p1 (47)] = 6;
    return p;
}

std::optional<DeviceDefinition::TuneSound> Hawk800Device::getTuneSound() const
{
    // DCO 1 alone with a single footage: a plain square wave (with one footage the waveform
    // setting makes no difference). Octave 16' + footage 16' = concert pitch (measured: note 69
    // plays 440 Hz; octave 8' plays 880 Hz). Filter fully open, no modulation, chorus off.
    auto p = createPatchWith ({ { "dco1_octave", 0 }, { "dco1_wave", 0 }, { "dco1_harmonics", 0 }, { "dco1_level", 31 },
                                { "dco_mode", 0 }, { "dco2_level", 0 }, { "noise", 0 }, { "chorus", 0 },
                                { "cutoff", 99 }, { "resonance", 0 }, { "eg3_int", 0 },
                                { "eg1_attack", 0 }, { "eg1_sustain", 31 }, { "key_assign", 0 }, { "portamento", 0 } });
    setPatchName (p, "Tune");
    return TuneSound { p, 69, TuneSound::Wave::square };
}

juce::String Hawk800Device::getPatchName (const Patch& p) const
{
    juce::String name;

    for (int i = 0; i < nameLength && synthPatchSize + i < (int) p.data.size(); ++i)
    {
        auto c = p.data[(size_t) (synthPatchSize + i)];

        if (c == 0)
            break;

        name << juce::String::charToString ((juce::juce_wchar) (c >= 32 && c < 127 ? c : ' '));
    }

    return name.trimEnd();
}

void Hawk800Device::setPatchName (Patch& p, const juce::String& name) const
{
    p.data.resize ((size_t) getPatchSize(), 0);

    for (int i = 0; i < nameLength; ++i)
    {
        auto c = i < name.length() ? (int) name[i] : 0;
        p.data[(size_t) (synthPatchSize + i)] = (uint8_t) (c == 0 || (c >= 32 && c < 127) ? c : '?');
    }
}

Patch Hawk800Device::asStored (const Patch& p) const
{
    // The synth keeps the sound, not our name
    auto stored = p;
    stored.data.resize ((size_t) getPatchSize(), 0);
    std::fill (stored.data.begin() + synthPatchSize, stored.data.end(), (uint8_t) 0);
    return stored;
}

//==============================================================================
juce::MidiMessage Hawk800Device::createSysEx (std::initializer_list<uint8_t> header, const uint8_t* bytes, int numBytes)
{
    std::vector<uint8_t> data { korg, poly800 };
    data.insert (data.end(), header.begin(), header.end());

    for (int i = 0; i < numBytes; ++i)
    {
        data.push_back ((uint8_t) (bytes[i] >> 4));
        data.push_back ((uint8_t) (bytes[i] & 0x0f));
    }

    return juce::MidiMessage::createSysExMessage (data.data(), (int) data.size());
}

std::vector<juce::MidiMessage> Hawk800Device::createParamMessages (const MidiSettings&, int paramIndex,
                                                                   const std::function<int (int)>& valueOf) const
{
    // One exact message per parameter: F0 42 21 0F <parameter> <stored value> F7
    auto& spec = params[(size_t) paramIndex];
    return { createSysEx ({ parameterChange, (uint8_t) spec.offset, (uint8_t) (spec.toStored (valueOf (paramIndex)) & 0x7f) }) };
}

juce::MidiMessage Hawk800Device::createEditBufferRequest (const MidiSettings&) const
{
    return createSysEx ({ requestPatch, (uint8_t) editBufferSlot });
}

juce::MidiMessage Hawk800Device::createEditBufferDump (const MidiSettings&, const Patch& patch) const
{
    jassert (patch.data.size() >= (size_t) synthPatchSize);
    return createSysEx ({ patchDump, (uint8_t) editBufferSlot }, patch.data.data(), synthPatchSize);
}

juce::MidiMessage Hawk800Device::createSlotRequest (const MidiSettings&, int slot) const
{
    return createSysEx ({ requestPatch, (uint8_t) (slot % slotsPerBank) });
}

juce::MidiMessage Hawk800Device::createSlotDump (const MidiSettings&, const Patch& patch, int slot) const
{
    jassert (patch.data.size() >= (size_t) synthPatchSize && juce::isPositiveAndBelow (slot, getNumSlots()));
    return createSysEx ({ patchDump, (uint8_t) (slot % slotsPerBank) }, patch.data.data(), synthPatchSize);
}

juce::MidiMessage Hawk800Device::createBankSelect (const MidiSettings&, int bank) const
{
    return createSysEx ({ bankChange, (uint8_t) juce::jlimit (0, numBanks - 1, bank) });
}

std::optional<DeviceDefinition::IncomingPatch> Hawk800Device::parsePatchDump (const MidiSettings&, const juce::MidiMessage& m) const
{
    // 42 21 06 <patch> + 256 nibbles
    auto* d = bodyOf (m, patchDump, 4 + 2 * synthPatchSize);

    if (d == nullptr || m.getSysExDataSize() != 4 + 2 * synthPatchSize || d[3] > editBufferSlot)
        return std::nullopt;

    IncomingPatch incoming;
    incoming.patch.data = fromNibbles (d + 4, 2 * synthPatchSize);
    incoming.patch.data.resize ((size_t) getPatchSize(), 0);
    incoming.slot = d[3] == editBufferSlot ? -1 : d[3];
    return incoming;
}

std::vector<DeviceDefinition::IncomingPatch> Hawk800Device::parseBulkDump (const MidiSettings&, const juce::MidiMessage& m) const
{
    // A patch bank for the flash (factory files, tools mode 8): 42 21 7F 00 <04-07 = bank 1-4>, then as nibbles
    // a checksum byte (sum of the data) and 64 patches of 128 bytes
    constexpr int numBytes = 1 + slotsPerBank * synthPatchSize;
    auto* d = bodyOf (m, flashUpload, 5 + 2 * numBytes);

    if (d == nullptr || d[3] != 0 || d[4] < 4 || d[4] > 7 || m.getSysExDataSize() != 5 + 2 * numBytes)
        return {};

    auto bytes = fromNibbles (d + 5, 2 * numBytes);
    int sum = 0;

    for (size_t i = 1; i < bytes.size(); ++i)
        sum += bytes[i];

    if ((sum & 0xff) != bytes[0])
        return {};

    std::vector<IncomingPatch> result;

    for (int i = 0; i < slotsPerBank; ++i)
    {
        IncomingPatch incoming;
        auto first = bytes.begin() + 1 + i * synthPatchSize;
        incoming.patch.data.assign (first, first + synthPatchSize);
        incoming.patch.data.resize ((size_t) getPatchSize(), 0);
        incoming.slot = i;
        result.push_back (std::move (incoming));
    }

    return result;
}

//==============================================================================
juce::MidiMessage Hawk800Device::createGlobalsRequest (const MidiSettings&) const
{
    return createSysEx ({ bulkRequest, 0x02 });
}

std::optional<std::vector<uint8_t>> Hawk800Device::parseGlobalsDump (const MidiSettings&, const juce::MidiMessage& m) const
{
    // 42 21 08 + 64 globals as nibbles, in the synth's numbering 11-88
    auto* d = bodyOf (m, globalsDump, 3 + 128);

    if (d == nullptr || m.getSysExDataSize() != 3 + 128)
        return std::nullopt;

    return fromNibbles (d + 3, 128);
}

int Hawk800Device::getSelectedBank (const std::vector<uint8_t>& globals) const
{
    // Global 13 = patch bank, stored 0-3
    return globals.size() > 2 && globals[2] < numBanks ? globals[2] : -1;
}

} // namespace sso::hawk800
