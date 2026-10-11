namespace sso::kiwisix
{

namespace
{
    using Rule = PatchGenerator::Rule;
    using RuleSet = PatchGenerator::RuleSet;

    // Rules are written in knob steps 0-127 and scaled to the 12-bit range
    constexpr int step = 32;
    constexpr int centre = 2048;

    Rule range (int lo, int hi)             { return Rule::range (lo * step, juce::jmin (hi * step, KiwiSixDevice::maxValue12)); }
    Rule fixed (int v)                      { return Rule::fixed (juce::jmin (v * step, KiwiSixDevice::maxValue12)); }
    Rule raw (int v)                        { return Rule::fixed (v); }
    Rule rawRange (int lo, int hi)          { return Rule::range (lo, hi); }
    Rule bipolar (int lo, int hi)           { return Rule::range (centre + lo * step, juce::jmin (centre + hi * step, KiwiSixDevice::maxValue12)); }
    Rule pick (std::vector<std::pair<int, float>> w) { return Rule::pick (std::move (w)); }
    Rule pickSteps (std::vector<std::pair<int, float>> w)
    {
        for (auto& [v, weight] : w)
            v = juce::jmin (v * step, KiwiSixDevice::maxValue12);

        return Rule::pick (std::move (w));
    }

    // choice indices (display order of the parameter's choices)
    enum { oct16 = 0, oct8 = 1, oct4 = 2 };
    enum { saw = 0, pw = 1, pwm = 2 };
    enum { subOff = 0, sub1 = 1, sub2 = 2 };
    enum { tri = 0, square = 1, sawWave = 2, revSaw = 3, random = 4 };
    enum { eg = 0, gate = 1 };
    enum { fxOff = 0, chorus = 1, phase = 2, ensemble = 3 };
    enum { poly1 = 0, poly2 = 1, unison = 2 };

    void envelope (RuleSet& r, Rule a, Rule d, Rule s, Rule rel)
    {
        r["eg_attack"] = a;
        r["eg_decay"] = d;
        r["eg_sustain"] = s;
        r["eg_release"] = rel;
    }

    RuleSet baseRules()
    {
        RuleSet r;

        r["vco_octave"] = raw (oct8);
        r["vco_wave"] = pick ({ { saw, 0.6f }, { pw, 0.2f }, { pwm, 0.2f } });
        r["pw_pwm"] = range (20, 90);
        r["pwm_speed"] = range (20, 60);
        r["pwm_wave"] = raw (tri);
        r["sub_osc"] = pick ({ { subOff, 0.7f }, { sub1, 0.3f } });

        r["cutoff"] = range (40, 85);
        r["resonance"] = range (0, 30);
        r["eg_int"] = bipolar (10, 40);
        r["kbd_track"] = range (50, 100);

        r["vca_level"] = rawRange (1850, 2050);     // as loud as the factory sounds (1983)
        r["vca_mode"] = raw (eg);

        envelope (r, range (0, 10), range (40, 80), range (60, 110), range (20, 50));

        r["mg_rate"] = range (70, 95);              // vibrato
        r["mg_delay"] = range (0, 40);
        r["mg_level"] = pickSteps ({ { 0, 0.6f }, { 6, 0.25f }, { 12, 0.15f } });
        r["mg_wave"] = raw (tri);
        r["mg_vco"] = raw (1);
        r["mg_vcf"] = raw (0);
        r["mg_vca"] = raw (0);

        r["fx_type"] = pick ({ { ensemble, 0.3f }, { chorus, 0.4f }, { fxOff, 0.2f }, { phase, 0.1f } });
        r["fx_speed"] = range (30, 70);

        r["key_assign"] = raw (poly1);
        r["hold"] = raw (0);
        r["arp_range"] = raw (0);
        r["arp_clock"] = raw (6);
        r["seq_clock"] = raw (6);
        return r;
    }

    RuleSet bass()
    {
        RuleSet r;
        r["vco_octave"] = pick ({ { oct16, 0.7f }, { oct8, 0.3f } });
        r["vco_wave"] = pick ({ { saw, 0.5f }, { pw, 0.3f }, { pwm, 0.2f } });
        r["sub_osc"] = pick ({ { sub1, 0.5f }, { sub2, 0.3f }, { subOff, 0.2f } });
        r["cutoff"] = range (25, 60);
        r["resonance"] = range (0, 80);
        r["eg_int"] = bipolar (20, 55);
        r["kbd_track"] = range (40, 90);
        r["vca_level"] = rawRange (2100, 2500);     // low notes need more level to sound as loud
        envelope (r, range (0, 4), range (25, 70), range (0, 45), range (8, 30));
        r["mg_level"] = raw (0);
        r["key_assign"] = pick ({ { poly1, 0.5f }, { unison, 0.5f } });
        r["fx_type"] = pick ({ { fxOff, 0.8f }, { chorus, 0.2f } });
        return r;
    }

    RuleSet poly()
    {
        RuleSet r;
        r["vco_wave"] = pick ({ { saw, 0.6f }, { pwm, 0.4f } });
        r["cutoff"] = range (40, 80);
        r["resonance"] = range (0, 40);
        r["eg_int"] = bipolar (15, 42);
        envelope (r, range (0, 15), range (40, 80), range (30, 90), range (15, 40));
        r["fx_type"] = pick ({ { chorus, 0.5f }, { ensemble, 0.3f }, { fxOff, 0.2f } });
        return r;
    }

    RuleSet pads()
    {
        RuleSet r;
        r["vco_octave"] = pick ({ { oct16, 0.3f }, { oct8, 0.7f } });
        r["vco_wave"] = pick ({ { pwm, 0.5f }, { saw, 0.5f } });
        r["pwm_speed"] = range (10, 40);
        r["cutoff"] = range (35, 75);
        r["resonance"] = range (0, 40);
        r["eg_int"] = bipolar (0, 25);
        envelope (r, range (40, 90), range (60, 100), range (80, 120), range (60, 110));
        r["mg_vcf"] = pick ({ { 0, 0.7f }, { 1, 0.3f } });
        r["fx_type"] = pick ({ { ensemble, 0.5f }, { chorus, 0.4f }, { phase, 0.1f } });
        r["fx_speed"] = range (20, 55);
        return r;
    }

    RuleSet fx()
    {
        RuleSet r;
        r["vco_octave"] = pick ({ { oct16, 1.0f }, { oct8, 1.0f }, { oct4, 1.0f } });
        r["vco_wave"] = pick ({ { saw, 1.0f }, { pw, 1.0f }, { pwm, 1.0f } });
        r["pw_pwm"] = range (0, 120);
        r["pwm_speed"] = range (0, 127);
        r["pwm_wave"] = pick ({ { tri, 1.0f }, { square, 1.0f }, { sawWave, 1.0f }, { revSaw, 1.0f }, { random, 1.0f } });
        r["sub_osc"] = pick ({ { subOff, 1.0f }, { sub1, 1.0f }, { sub2, 1.0f } });
        r["cutoff"] = range (20, 100);
        r["resonance"] = range (30, 120);
        r["eg_int"] = range (0, 127);
        envelope (r, range (0, 110), range (0, 110), range (0, 127), range (0, 110));
        r["mg_rate"] = range (10, 127);
        r["mg_delay"] = range (0, 60);
        r["mg_level"] = range (20, 100);
        r["mg_wave"] = pick ({ { tri, 1.0f }, { square, 1.0f }, { sawWave, 1.0f }, { revSaw, 1.0f }, { random, 1.0f } });
        r["mg_vco"] = pick ({ { 0, 0.5f }, { 1, 0.5f } });
        r["mg_vcf"] = pick ({ { 0, 0.5f }, { 1, 0.5f } });
        r["mg_vca"] = pick ({ { 0, 0.7f }, { 1, 0.3f } });
        r["fx_type"] = pick ({ { fxOff, 1.0f }, { chorus, 1.0f }, { phase, 1.0f }, { ensemble, 1.0f } });
        r["fx_speed"] = range (0, 127);
        r["key_assign"] = pick ({ { poly1, 0.7f }, { unison, 0.3f } });
        return r;
    }

    RuleSet keys()
    {
        RuleSet r;
        r["vco_wave"] = pick ({ { pw, 0.5f }, { saw, 0.3f }, { pwm, 0.2f } });
        r["pw_pwm"] = range (30, 80);
        r["cutoff"] = range (40, 80);
        r["resonance"] = range (0, 50);
        r["eg_int"] = bipolar (25, 55);
        r["kbd_track"] = range (70, 110);
        envelope (r, range (0, 2), range (40, 75), range (0, 30), range (15, 40));
        r["fx_type"] = pick ({ { chorus, 0.5f }, { fxOff, 0.5f } });
        return r;
    }

    RuleSet strings()
    {
        RuleSet r;
        r["vco_octave"] = pick ({ { oct8, 0.7f }, { oct16, 0.3f } });
        r["vco_wave"] = pick ({ { saw, 0.5f }, { pwm, 0.5f } });
        r["pwm_speed"] = range (30, 60);
        r["cutoff"] = range (50, 90);
        r["resonance"] = range (0, 20);
        r["eg_int"] = bipolar (0, 15);
        envelope (r, range (30, 60), range (60, 90), range (100, 127), range (40, 80));
        r["mg_delay"] = range (30, 70);
        r["mg_level"] = range (5, 12);
        r["fx_type"] = pick ({ { ensemble, 0.8f }, { chorus, 0.2f } });
        return r;
    }

    RuleSet bells()
    {
        RuleSet r;
        r["vco_octave"] = pick ({ { oct4, 0.5f }, { oct8, 0.5f } });
        r["vco_wave"] = pick ({ { pw, 0.6f }, { saw, 0.4f } });
        r["pw_pwm"] = range (60, 110);
        r["cutoff"] = range (60, 110);
        r["resonance"] = range (0, 30);
        r["eg_int"] = bipolar (0, 20);
        r["kbd_track"] = range (80, 127);
        envelope (r, range (0, 2), range (60, 100), range (0, 20), range (60, 100));
        r["fx_type"] = pick ({ { chorus, 0.6f }, { phase, 0.4f } });
        return r;
    }

    RuleSet brass()
    {
        RuleSet r;
        r["vco_octave"] = pick ({ { oct8, 0.75f }, { oct16, 0.25f } });
        r["vco_wave"] = raw (saw);
        r["cutoff"] = range (25, 55);
        r["resonance"] = range (0, 25);
        r["eg_int"] = bipolar (25, 50);
        envelope (r, range (10, 35), range (50, 80), range (50, 90), range (25, 45));
        r["fx_type"] = pick ({ { fxOff, 0.5f }, { chorus, 0.5f } });
        return r;
    }
}

std::unique_ptr<PatchGenerator> createGenerator (const KiwiSixDevice& device)
{
    auto g = std::make_unique<PatchGenerator> (device);

    g->assignGroup ("VCO", "VCO");
    g->assignGroup ("VCF", "VCF");
    g->assignGroup ("VCA", "VCA");
    g->assignGroup ("EG", "EG");
    g->assignGroup ("MG", "MG");
    g->assignGroup ("Effects", "FX");
    for (auto* s : { "Key Assign", "Arpeggiator" })   g->assignGroup (s, "VOICE");

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

        if (get ("vca_level") < 1500)
            set ("vca_level", 1983);

        // An envelope that decays to silence needs some decay time to be heard
        if (get ("vca_mode") == eg && get ("eg_sustain") < 20 * step && get ("eg_decay") < 30 * step)
            set ("eg_decay", 30 * step);

        // The filter must open at least a bit (cutoff plus positive envelope amount)
        if (get ("cutoff") + juce::jmax (0, get ("eg_int") - centre) < 35 * step)
            set ("cutoff", 35 * step);

        p = device.asStored (p);
    };

    return g;
}

} // namespace sso::kiwisix
