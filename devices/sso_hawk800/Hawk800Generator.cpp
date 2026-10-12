namespace sso::hawk800
{

namespace
{
    using Rule = PatchGenerator::Rule;
    using RuleSet = PatchGenerator::RuleSet;

    // Rules are in stored values (EG 0-31, cutoff/resonance 0-99, depths 0-15) and follow the
    // original Korg sounds (factory bank 2) of each kind.
    Rule range (int lo, int hi)                      { return Rule::range (lo, hi); }
    Rule fixed (int v)                               { return Rule::fixed (v); }
    Rule pick (std::vector<std::pair<int, float>> w) { return Rule::pick (std::move (w)); }

    // choice indices
    enum { oct16 = 0, oct8 = 1, oct4 = 2 };
    enum { square = 0, saw = 1 };
    enum { h16 = 0, h8 = 1, h4 = 2, h2 = 3, h16_8 = 4, h16_4 = 5, h16_2 = 6, h8_4 = 7, h8_2 = 8, h4_2 = 9,
           h16_8_4 = 10, h16_8_2 = 11, h16_4_2 = 12, h8_4_2 = 13, hAll = 14, hOff = 15 };
    enum { single = 0, doubled = 1 };
    enum { negative = 0, positive = 1 };

    /** Both amp envelopes (EG1 -> DCO 1, EG2 -> DCO 2) */
    void ampEnvelope (RuleSet& r, Rule a, Rule d, Rule bp, Rule sl, Rule s, Rule rel)
    {
        for (auto* eg : { "eg1", "eg2" })
        {
            auto id = [eg] (const char* p) { return juce::String (eg) + p; };
            r[id ("_attack")] = a; r[id ("_decay")] = d; r[id ("_break")] = bp;
            r[id ("_slope")] = sl; r[id ("_sustain")] = s; r[id ("_release")] = rel;
        }
    }

    void filterEnvelope (RuleSet& r, Rule a, Rule d, Rule bp, Rule sl, Rule s, Rule rel)
    {
        r["eg3_attack"] = a; r["eg3_decay"] = d; r["eg3_break"] = bp;
        r["eg3_slope"] = sl; r["eg3_sustain"] = s; r["eg3_release"] = rel;
    }

    void oscillators (RuleSet& r, Rule octave, Rule wave, Rule footages)
    {
        for (auto* dco : { "dco1", "dco2" })
        {
            r[juce::String (dco) + "_octave"] = octave;
            r[juce::String (dco) + "_wave"] = wave;
            r[juce::String (dco) + "_harmonics"] = footages;
        }
    }

    RuleSet baseRules()
    {
        RuleSet r;
        oscillators (r, pick ({ { oct8, 0.45f }, { oct4, 0.55f } }), pick ({ { saw, 0.7f }, { square, 0.3f } }),
                     pick ({ { hAll, 0.5f }, { h16_4_2, 0.15f }, { h16_8, 0.1f }, { h4_2, 0.1f }, { h16_8_2, 0.1f }, { h8, 0.05f } }));
        r["dco1_level"] = range (14, 24);
        r["dco2_level"] = range (12, 24);
        r["dco_mode"] = pick ({ { doubled, 0.75f }, { single, 0.25f } });
        r["dco2_interval"] = pick ({ { 0, 0.85f }, { 7, 0.1f }, { 12, 0.05f } });
        r["dco2_detune"] = pick ({ { 2, 0.5f }, { 1, 0.3f }, { 3, 0.2f } });
        r["noise"] = fixed (0);
        r["chorus"] = pick ({ { 1, 0.85f }, { 0, 0.15f } });

        r["cutoff"] = range (30, 75);
        r["resonance"] = pick ({ { 1, 0.4f }, { 14, 0.3f }, { 33, 0.2f }, { 53, 0.1f } });
        r["kbd_track"] = pick ({ { 0, 0.4f }, { 1, 0.45f }, { 2, 0.15f } });
        r["eg3_polarity"] = fixed (positive);
        r["eg3_int"] = range (0, 10);
        r["eg3_trigger"] = fixed (0);

        ampEnvelope (r, range (0, 6), range (20, 31), range (16, 31), range (17, 28), range (13, 31), range (15, 20));
        filterEnvelope (r, range (0, 13), range (10, 24), range (13, 30), range (14, 26), range (10, 24), range (14, 20));

        r["lfo1_rate"] = range (6, 9);              // vibrato
        r["lfo1_delay"] = pick ({ { 0, 0.4f }, { 4, 0.3f }, { 8, 0.3f } });
        r["lfo2_rate"] = range (6, 9);
        r["dco_lfo_depth"] = pick ({ { 0, 0.6f }, { 1, 0.25f }, { 2, 0.15f } });
        return r;
    }

    RuleSet bass()
    {
        RuleSet r;
        oscillators (r, pick ({ { oct16, 0.3f }, { oct8, 0.6f }, { oct4, 0.1f } }), pick ({ { saw, 0.6f }, { square, 0.4f } }),
                     pick ({ { hAll, 0.4f }, { h16_8, 0.3f }, { h16_8_4, 0.15f }, { h16_8_2, 0.15f } }));
        r["dco_mode"] = fixed (doubled);
        r["dco2_interval"] = fixed (0);
        r["chorus"] = pick ({ { 1, 0.6f }, { 0, 0.4f } });
        r["cutoff"] = range (5, 40);
        r["resonance"] = range (1, 30);
        r["eg3_int"] = range (6, 12);
        ampEnvelope (r, fixed (0), range (15, 27), range (14, 26), range (13, 27), pick ({ { 0, 0.6f }, { 8, 0.2f }, { 24, 0.2f } }), range (16, 20));
        filterEnvelope (r, fixed (0), range (15, 24), range (10, 16), range (24, 26), fixed (0), range (16, 24));
        r["dco_lfo_depth"] = fixed (0);
        return r;
    }

    RuleSet poly()
    {
        RuleSet r;
        r["cutoff"] = range (20, 40);
        r["resonance"] = range (14, 40);
        r["eg3_int"] = range (6, 12);
        ampEnvelope (r, range (0, 6), range (20, 31), range (16, 31), range (17, 28), range (13, 31), range (15, 19));
        filterEnvelope (r, range (0, 13), range (0, 24), range (13, 30), range (0, 31), range (19, 31), range (14, 19));
        return r;
    }

    RuleSet pads()
    {
        RuleSet r;
        r["cutoff"] = range (45, 75);
        r["resonance"] = pick ({ { 1, 0.4f }, { 14, 0.3f }, { 20, 0.3f } });
        r["eg3_int"] = range (0, 4);
        r["chorus"] = fixed (1);
        ampEnvelope (r, range (9, 24), range (24, 31), range (14, 31), range (15, 31), range (21, 31), range (17, 24));
        filterEnvelope (r, range (0, 12), range (10, 23), range (0, 27), range (0, 25), range (9, 21), range (6, 20));
        r["dco_lfo_depth"] = pick ({ { 1, 0.5f }, { 0, 0.3f }, { 3, 0.2f } });
        r["lfo1_delay"] = range (2, 8);
        return r;
    }

    RuleSet fx()
    {
        RuleSet r;
        oscillators (r, pick ({ { oct16, 1.0f }, { oct8, 1.0f }, { oct4, 1.0f } }), pick ({ { saw, 1.0f }, { square, 1.0f } }),
                     range (h16, h8_4_2 + 1));
        r["dco2_interval"] = range (0, 12);
        r["dco2_detune"] = range (0, 3);
        r["noise"] = pick ({ { 0, 0.5f }, { 10, 0.25f }, { 15, 0.25f } });
        r["cutoff"] = range (0, 99);
        r["resonance"] = range (14, 98);
        r["eg3_polarity"] = pick ({ { positive, 0.6f }, { negative, 0.4f } });
        r["eg3_int"] = range (0, 15);
        ampEnvelope (r, range (0, 30), range (0, 31), range (0, 31), range (0, 31), range (0, 31), range (0, 31));
        filterEnvelope (r, range (0, 30), range (0, 31), range (0, 31), range (0, 31), range (0, 31), range (0, 31));
        r["lfo1_rate"] = range (0, 15);
        r["lfo2_rate"] = range (0, 15);
        r["dco_lfo_wave"] = range (0, 12);
        r["dco_lfo_source"] = range (0, 1);
        r["dco_lfo_depth"] = pick ({ { 0, 0.4f }, { 4, 0.3f }, { 15, 0.3f } });
        r["vcf_lfo1_wave"] = range (0, 12);
        r["vcf_lfo1_source"] = range (0, 1);
        r["vcf_lfo1_depth"] = pick ({ { 0, 0.5f }, { 8, 0.25f }, { 15, 0.25f } });
        return r;
    }

    RuleSet keys()
    {
        RuleSet r;
        oscillators (r, pick ({ { oct4, 0.8f }, { oct8, 0.2f } }), pick ({ { saw, 0.75f }, { square, 0.25f } }),
                     pick ({ { hAll, 0.5f }, { h16_8, 0.3f }, { h16_4_2, 0.2f } }));
        r["dco_mode"] = pick ({ { single, 0.7f }, { doubled, 0.3f } });
        r["cutoff"] = range (41, 68);
        r["resonance"] = pick ({ { 1, 0.3f }, { 33, 0.4f }, { 53, 0.3f } });
        r["eg3_int"] = range (0, 9);
        ampEnvelope (r, fixed (0), range (11, 22), range (18, 24), range (25, 27), fixed (0), range (14, 24));
        filterEnvelope (r, fixed (0), range (16, 22), range (15, 30), fixed (25), range (10, 24), range (0, 30));
        r["dco_lfo_depth"] = fixed (0);
        return r;
    }

    RuleSet strings()
    {
        RuleSet r;
        oscillators (r, pick ({ { oct4, 0.6f }, { oct8, 0.4f } }), fixed (saw),
                     pick ({ { hAll, 0.6f }, { h16_4_2, 0.25f }, { h8_2, 0.15f } }));
        r["dco_mode"] = fixed (doubled);
        r["dco2_detune"] = fixed (2);
        r["cutoff"] = range (51, 78);
        r["resonance"] = pick ({ { 1, 0.6f }, { 14, 0.2f }, { 20, 0.2f } });
        r["eg3_int"] = range (0, 3);
        r["chorus"] = fixed (1);
        ampEnvelope (r, range (9, 24), range (24, 31), range (0, 28), range (1, 31), range (21, 31), range (17, 24));
        filterEnvelope (r, range (0, 12), range (10, 23), range (0, 29), range (0, 25), range (9, 21), range (6, 22));
        r["dco_lfo_depth"] = pick ({ { 1, 0.6f }, { 3, 0.2f }, { 0, 0.2f } });
        r["lfo1_delay"] = range (0, 8);
        return r;
    }

    RuleSet bells()
    {
        RuleSet r;
        oscillators (r, fixed (oct4), pick ({ { square, 0.7f }, { saw, 0.3f } }),
                     pick ({ { h8_2, 0.3f }, { h4_2, 0.3f }, { h8, 0.2f }, { h8_4, 0.2f } }));
        r["dco2_interval"] = pick ({ { 0, 0.4f }, { 7, 0.2f }, { 9, 0.2f }, { 12, 0.2f } });
        r["dco2_detune"] = range (0, 1);
        r["cutoff"] = range (60, 99);
        r["resonance"] = fixed (1);
        r["eg3_int"] = fixed (0);
        ampEnvelope (r, fixed (0), range (15, 24), range (21, 31), range (23, 27), fixed (0), range (21, 24));
        r["dco_lfo_depth"] = pick ({ { 0, 0.5f }, { 1, 0.5f } });
        return r;
    }

    RuleSet brass()
    {
        RuleSet r;
        oscillators (r, pick ({ { oct4, 0.8f }, { oct8, 0.2f } }), fixed (saw), fixed (hAll));
        r["dco_mode"] = pick ({ { doubled, 0.6f }, { single, 0.4f } });
        r["cutoff"] = range (0, 29);
        r["resonance"] = pick ({ { 1, 0.4f }, { 14, 0.4f }, { 27, 0.2f } });
        r["kbd_track"] = fixed (1);
        r["eg3_int"] = range (7, 12);
        ampEnvelope (r, range (0, 10), range (22, 31), range (23, 31), range (17, 31), range (21, 31), range (16, 19));
        filterEnvelope (r, range (7, 24), range (11, 25), range (21, 26), range (24, 26), range (11, 20), range (16, 20));
        r["dco_lfo_depth"] = pick ({ { 0, 0.5f }, { 2, 0.3f }, { 3, 0.2f } });
        return r;
    }
}

std::unique_ptr<PatchGenerator> createGenerator (const Hawk800Device& device)
{
    auto g = std::make_unique<PatchGenerator> (device);

    for (auto* s : { "DCO 1", "DCO 2", "Noise" })      g->assignGroup (s, "DCO");
    g->assignGroup ("VCF", "VCF");
    for (auto* s : { "EG 1", "EG 2", "EG 3" })          g->assignGroup (s, "EG");
    g->assignGroup ("LFO", "LFO");
    g->assignGroup ("Modulation", "MOD");
    g->assignGroup ("Velocity", "VEL");
    g->assignGroup ("Play", "PLAY");

    g->setBaseRules (baseRules());
    g->addCategory ("Bass",    bass(),    { "Bass", "Sub", "Low End", "Thump", "Bassline" });
    g->addCategory ("Poly",    poly(),    { "Poly", "Stab", "Synth", "Chords", "Hook" });
    g->addCategory ("Pads",    pads(),    { "Pad", "Haze", "Cloud", "Drift", "Wash" });
    g->addCategory ("FX",      fx(),      { "Zap", "Sweep", "Drone", "Blip", "Space" });
    g->addCategory ("Keys",    keys(),    { "Keys", "Clav", "Piano", "Tines", "Pluck" });
    g->addCategory ("Strings", strings(), { "Strings", "Ensemble", "Violins", "Section" });
    g->addCategory ("Bells",   bells(),   { "Bells", "Chime", "Mallet", "Glock" });
    g->addCategory ("Brass",   brass(),   { "Brass", "Horns", "Fanfare", "Brass Section" });

    // Keep everything playable, whatever was generated or kept.
    g->sanitise = [&device] (Patch& p, juce::Random&)
    {
        auto get = [&] (const char* id) { return device.getValue (p, device.indexOf (id)); };
        auto set = [&] (const char* id, int v) { device.setValue (p, device.indexOf (id), v); };

        for (auto* dco : { "dco1", "dco2" })
            if (get ((juce::String (dco) + "_harmonics").toRawUTF8()) == hOff)
                set ((juce::String (dco) + "_harmonics").toRawUTF8(), hAll);

        if (get ("dco1_level") < 8)
            set ("dco1_level", 16);

        if (get ("dco_mode") == doubled && get ("dco2_level") < 8)
            set ("dco2_level", 16);

        // A sound that decays to silence needs some decay time to be heard
        for (auto* eg : { "eg1", "eg2" })
        {
            auto id = [eg] (const char* s) { return (juce::String (eg) + s); };

            if (get (id ("_sustain").toRawUTF8()) < 8 && get (id ("_decay").toRawUTF8()) < 12)
                set (id ("_decay").toRawUTF8(), 15);
        }

        // The filter must open at least a bit (cutoff plus a positive filter envelope)
        auto opening = get ("eg3_polarity") == positive ? get ("eg3_int") * 5 : 0;

        if (get ("cutoff") + opening < 20)
            set ("cutoff", 20);
    };

    return g;
}

} // namespace sso::hawk800
