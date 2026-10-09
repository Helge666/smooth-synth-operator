namespace sso::kiwi3p
{

namespace
{
    using Rule = PatchGenerator::Rule;
    using RuleSet = PatchGenerator::RuleSet;

    constexpr int centre = 63;

    // DCO tune value for a transposition in semitones (range +-12)
    int tuneFor (int st)                  { return centre + juce::roundToInt (st * 63.0 / 12.0); }

    Rule range (int lo, int hi)             { return Rule::range (lo, hi); }
    Rule fixed (int v)                      { return Rule::fixed (v); }
    Rule bipolar (int lo, int hi)           { return Rule::range (centre + lo, centre + hi); }
    Rule pick (std::vector<std::pair<int, float>> w) { return Rule::pick (std::move (w)); }

    // choice indices (display order of the parameter's choices)
    enum { range16 = 0, range8 = 1, range4 = 2 };
    enum { saw = 0, pulse = 1, square = 2, noise = 3 };
    enum { lfoSine = 0, lfoTri = 1, lfoSaw = 2, lfoRevSaw = 3, lfoSquare = 4, lfoRandom = 5, lfoRandom2 = 6 };
    enum { gate = 0, env1 = 1, env2 = 2 };
    enum { chorusOff = 0, chorusAuto = 1, chorusManual = 2 };
    enum { poly1 = 0, poly2 = 1, unison1 = 2, unison2 = 3, mono1 = 4, mono2 = 5 };
    enum { off = 0, first = 1, second = 2, both = 3 };

    void envelope (RuleSet& r, const char* env, Rule a, Rule d, Rule s, Rule rel)
    {
        auto p = juce::String (env);
        r[p + "_attack"] = a;
        r[p + "_decay"] = d;
        r[p + "_sustain"] = s;
        r[p + "_release"] = rel;
    }

    RuleSet baseRules()
    {
        RuleSet r;

        r["dco1_range"] = fixed (range8);
        r["dco1_wave"] = pick ({ { saw, 0.6f }, { pulse, 0.3f }, { square, 0.1f } });
        r["dco1_tune"] = fixed (centre);
        r["dco1_lfo"] = fixed (0);
        r["dco1_lfo_src"] = fixed (0);
        r["dco1_env"] = fixed (centre);
        r["dco1_env_src"] = fixed (0);

        r["dco2_range"] = fixed (range8);
        r["dco2_wave"] = pick ({ { saw, 0.6f }, { pulse, 0.3f }, { square, 0.1f } });
        r["dco2_tune"] = fixed (centre);
        r["dco2_fine"] = range (52, 74);
        r["dco2_lfo"] = fixed (0);
        r["dco2_lfo_src"] = fixed (0);
        r["dco2_env"] = fixed (centre);
        r["dco2_env_src"] = fixed (0);
        r["xmod"] = fixed (0);

        r["dco_mix"] = range (48, 80);
        r["env_velocity"] = fixed (off);

        r["hpf"] = range (0, 8);
        r["cutoff"] = range (40, 80);
        r["resonance"] = range (0, 30);
        r["vcf_lfo"] = fixed (0);
        r["vcf_lfo_src"] = fixed (1);
        r["vcf_env"] = bipolar (15, 40);
        r["vcf_env_src"] = fixed (0);           // ENV-1 -> filter
        r["key_follow"] = range (50, 90);

        r["vca_mode"] = fixed (env2);           // ENV-2 -> amp
        r["vca_level"] = range (45, 62);        // as loud as the factory sounds (VCA median 62, but brighter)
        r["vca_lfo"] = fixed (0);
        r["vca_lfo_src"] = fixed (1);

        envelope (r, "env1", range (0, 10), range (40, 80), range (30, 80), range (20, 50));
        envelope (r, "env2", range (0, 5), range (50, 90), range (90, 127), range (20, 50));
        r["env_speed"] = fixed (0);

        r["lfo1_wave"] = fixed (lfoTri);        // vibrato
        r["lfo1_rate"] = range (88, 104);
        r["lfo1_delay"] = range (0, 40);
        r["lfo_mode"] = fixed (0);
        r["lfo2_wave"] = pick ({ { lfoSine, 0.4f }, { lfoTri, 0.4f }, { lfoRandom, 0.2f } });
        r["lfo2_rate"] = range (20, 60);
        r["lfo2_delay"] = fixed (0);

        r["chorus"] = pick ({ { chorusOff, 0.4f }, { chorusAuto, 0.6f } });
        r["chorus_rate"] = range (30, 70);

        r["voice_mode"] = fixed (poly1);
        r["detune"] = fixed (0);
        r["portamento"] = fixed (0);

        r["bend_dco"] = fixed (both);
        for (auto* id : { "bend_mix", "bend_cutoff", "bend_lfo", "bend_vca" })
            r[id] = fixed (0);

        // Mod wheel = vibrato (LFO-1 depth on both DCOs), aftertouch = a little brightness
        r["mw_depth"] = bipolar (30, 50);
        r["lfo_button"] = range (60, 80);
        r["at_depth"] = bipolar (15, 30);

        for (auto* prefix : { "mw", "at" })
            for (auto* dest : { "_dco_pitch", "_dco_lfo", "_dco_env", "_mix", "_vcf_lfo", "_vcf_env", "_cutoff",
                                "_reso", "_hpf", "_vca_level", "_vca_lfo", "_lfo_rate", "_chorus" })
                r[juce::String (prefix) + dest] = fixed (0);

        r["mw_dco_lfo"] = fixed (both);
        r["at_cutoff"] = pick ({ { 0, 0.5f }, { 1, 0.5f } });

        r["load_seq"] = fixed (0);
        r["load_pattern"] = fixed (0);
        return r;
    }

    RuleSet bass()
    {
        RuleSet r;
        r["dco1_range"] = pick ({ { range16, 0.7f }, { range8, 0.3f } });
        r["dco2_range"] = pick ({ { range16, 0.6f }, { range8, 0.4f } });
        r["dco1_wave"] = pick ({ { saw, 0.5f }, { pulse, 0.3f }, { square, 0.2f } });
        r["dco2_wave"] = pick ({ { saw, 0.5f }, { pulse, 0.3f }, { square, 0.2f } });
        r["dco2_tune"] = pick ({ { centre, 0.75f }, { tuneFor (12), 0.15f }, { tuneFor (7), 0.1f } });
        r["dco2_fine"] = range (57, 69);
        r["xmod"] = pick ({ { 0, 0.85f }, { 1, 0.15f } });
        r["cutoff"] = range (15, 55);
        r["resonance"] = range (0, 85);
        r["vcf_env"] = bipolar (20, 58);
        r["key_follow"] = range (40, 90);
        envelope (r, "env1", range (0, 4), range (25, 70), range (0, 45), range (8, 30));
        envelope (r, "env2", range (0, 3), range (40, 80), range (80, 127), range (8, 30));
        r["voice_mode"] = pick ({ { poly1, 0.3f }, { mono2, 0.5f }, { unison1, 0.2f } });
        r["detune"] = range (0, 25);
        r["portamento"] = pick ({ { 0, 0.7f }, { 15, 0.1f }, { 25, 0.1f }, { 35, 0.1f } });
        r["chorus"] = pick ({ { chorusOff, 0.8f }, { chorusAuto, 0.2f } });
        return r;
    }

    RuleSet poly()
    {
        RuleSet r;
        r["dco1_wave"] = pick ({ { saw, 0.6f }, { pulse, 0.4f } });
        r["dco2_wave"] = pick ({ { saw, 0.6f }, { pulse, 0.4f } });
        r["dco2_tune"] = pick ({ { centre, 0.8f }, { tuneFor (12), 0.1f }, { tuneFor (7), 0.1f } });
        r["cutoff"] = range (40, 80);
        r["resonance"] = range (0, 40);
        r["vcf_env"] = bipolar (15, 42);
        envelope (r, "env1", range (0, 15), range (40, 80), range (30, 80), range (20, 50));
        envelope (r, "env2", range (0, 10), range (50, 90), range (90, 127), range (15, 40));
        r["chorus"] = pick ({ { chorusAuto, 0.75f }, { chorusOff, 0.25f } });
        return r;
    }

    RuleSet pads()
    {
        RuleSet r;
        r["dco1_range"] = pick ({ { range16, 0.3f }, { range8, 0.7f } });
        r["dco1_wave"] = pick ({ { saw, 0.7f }, { pulse, 0.3f } });
        r["dco2_wave"] = pick ({ { saw, 0.7f }, { pulse, 0.3f } });
        r["dco2_tune"] = pick ({ { centre, 0.7f }, { tuneFor (7), 0.15f }, { tuneFor (12), 0.15f } });
        r["dco2_fine"] = range (50, 76);
        r["dco2_lfo"] = range (0, 6);
        r["dco2_lfo_src"] = fixed (1);
        r["cutoff"] = range (35, 75);
        r["resonance"] = range (0, 40);
        r["vcf_env"] = bipolar (0, 25);
        r["vcf_lfo"] = range (0, 20);
        envelope (r, "env1", range (40, 90), range (60, 100), range (60, 110), range (60, 110));
        envelope (r, "env2", range (40, 100), range (60, 100), range (100, 127), range (60, 115));
        r["lfo2_rate"] = range (15, 45);
        r["chorus"] = pick ({ { chorusAuto, 0.85f }, { chorusManual, 0.15f } });
        return r;
    }

    RuleSet fx()
    {
        RuleSet r;
        r["dco1_range"] = pick ({ { range16, 1.0f }, { range8, 1.0f }, { range4, 1.0f } });
        r["dco2_range"] = pick ({ { range16, 1.0f }, { range8, 1.0f }, { range4, 1.0f } });
        r["dco1_wave"] = pick ({ { saw, 1.0f }, { pulse, 1.0f }, { square, 1.0f } });
        r["dco2_wave"] = pick ({ { saw, 1.0f }, { pulse, 1.0f }, { square, 1.0f }, { noise, 0.7f } });
        r["dco2_tune"] = range (0, 126);
        r["dco2_env"] = range (0, 126);                 // pitch sweeps
        r["dco2_env_src"] = pick ({ { 0, 1.0f }, { 1, 1.0f } });
        r["xmod"] = pick ({ { 0, 0.3f }, { 1, 0.4f }, { 2, 0.3f } });
        r["dco1_lfo"] = pick ({ { 0, 0.5f }, { 20, 0.2f }, { 45, 0.2f }, { 70, 0.1f } });
        r["dco1_lfo_src"] = pick ({ { 0, 1.0f }, { 1, 1.0f } });
        r["dco2_lfo"] = range (0, 80);
        r["dco2_lfo_src"] = pick ({ { 0, 1.0f }, { 1, 1.0f } });
        r["cutoff"] = range (20, 100);
        r["resonance"] = range (30, 110);
        r["vcf_lfo"] = range (0, 90);
        r["vcf_env"] = range (10, 126);
        envelope (r, "env1", range (0, 110), range (0, 110), range (0, 127), range (0, 110));
        envelope (r, "env2", range (0, 60), range (30, 110), range (40, 127), range (20, 110));
        r["lfo1_wave"] = pick ({ { lfoSine, 1.0f }, { lfoTri, 1.0f }, { lfoSaw, 1.0f }, { lfoRevSaw, 1.0f }, { lfoSquare, 1.0f }, { lfoRandom, 1.0f } });
        r["lfo1_rate"] = range (10, 120);
        r["lfo2_wave"] = pick ({ { lfoSine, 1.0f }, { lfoTri, 1.0f }, { lfoSaw, 1.0f }, { lfoRevSaw, 1.0f }, { lfoSquare, 1.0f }, { lfoRandom, 1.0f }, { lfoRandom2, 0.5f } });
        r["lfo2_rate"] = range (5, 127);
        r["chorus"] = pick ({ { chorusOff, 1.0f }, { chorusAuto, 1.0f }, { chorusManual, 1.0f } });
        r["voice_mode"] = pick ({ { poly1, 0.6f }, { unison1, 0.2f }, { unison2, 0.2f } });
        r["portamento"] = pick ({ { 0, 0.6f }, { 40, 0.2f }, { 70, 0.2f } });
        return r;
    }

    RuleSet keys()
    {
        RuleSet r;
        r["dco1_wave"] = pick ({ { pulse, 0.5f }, { square, 0.3f }, { saw, 0.2f } });
        r["dco2_wave"] = pick ({ { pulse, 0.4f }, { square, 0.3f }, { saw, 0.3f } });
        r["dco2_tune"] = pick ({ { centre, 0.8f }, { tuneFor (12), 0.2f } });
        r["cutoff"] = range (40, 80);
        r["resonance"] = range (0, 50);
        r["vcf_env"] = bipolar (25, 55);
        r["key_follow"] = range (70, 110);
        envelope (r, "env1", range (0, 2), range (40, 75), range (0, 30), range (15, 40));
        envelope (r, "env2", range (0, 2), range (50, 85), range (0, 45), range (20, 45));
        r["chorus"] = pick ({ { chorusOff, 0.5f }, { chorusAuto, 0.5f } });
        return r;
    }

    RuleSet strings()
    {
        RuleSet r;
        r["dco1_range"] = pick ({ { range8, 0.7f }, { range16, 0.3f } });
        r["dco1_wave"] = fixed (saw);
        r["dco2_wave"] = fixed (saw);
        r["dco2_fine"] = range (50, 76);
        r["hpf"] = range (10, 40);
        r["cutoff"] = range (50, 90);
        r["resonance"] = range (0, 20);
        r["vcf_env"] = bipolar (0, 15);
        envelope (r, "env1", range (30, 60), range (60, 90), range (80, 110), range (40, 80));
        envelope (r, "env2", range (30, 70), range (60, 90), range (110, 127), range (45, 90));
        r["lfo1_delay"] = range (30, 70);
        r["chorus"] = fixed (chorusAuto);
        return r;
    }

    RuleSet bells()
    {
        RuleSet r;
        r["dco1_range"] = pick ({ { range8, 0.5f }, { range4, 0.5f } });
        r["dco1_wave"] = pick ({ { square, 0.5f }, { pulse, 0.5f } });
        r["dco2_wave"] = pick ({ { square, 0.5f }, { pulse, 0.5f } });
        r["dco2_tune"] = pick ({ { tuneFor (12), 0.35f }, { tuneFor (7), 0.25f }, { tuneFor (5), 0.15f }, { centre, 0.25f } });
        r["xmod"] = pick ({ { 0, 0.6f }, { 2, 0.4f } });
        r["cutoff"] = range (60, 110);
        r["resonance"] = range (0, 30);
        r["vcf_env"] = bipolar (0, 20);
        r["key_follow"] = range (80, 127);
        envelope (r, "env1", range (0, 2), range (60, 100), range (0, 20), range (60, 100));
        envelope (r, "env2", range (0, 2), range (70, 110), range (0, 15), range (60, 110));
        r["chorus"] = fixed (chorusAuto);
        return r;
    }

    RuleSet brass()
    {
        RuleSet r;
        r["dco1_range"] = pick ({ { range8, 0.75f }, { range16, 0.25f } });
        r["dco1_wave"] = fixed (saw);
        r["dco2_wave"] = fixed (saw);
        r["dco2_fine"] = range (56, 70);
        r["cutoff"] = range (25, 55);
        r["resonance"] = range (0, 25);
        r["vcf_env"] = bipolar (25, 50);
        envelope (r, "env1", range (10, 35), range (50, 80), range (50, 90), range (25, 45));
        envelope (r, "env2", range (5, 20), range (40, 80), range (100, 127), range (25, 45));
        r["chorus"] = pick ({ { chorusOff, 0.5f }, { chorusAuto, 0.5f } });
        return r;
    }
}

std::unique_ptr<PatchGenerator> createGenerator (const Kiwi3PDevice& device)
{
    auto g = std::make_unique<PatchGenerator> (device);

    for (auto* s : { "DCO 1", "DCO 2", "Mixer" })              g->assignGroup (s, "DCO");
    g->assignGroup ("VCF", "VCF");
    g->assignGroup ("VCA", "VCA");
    for (auto* s : { "ENV 1", "ENV 2" })                       g->assignGroup (s, "ENV");
    for (auto* s : { "LFO 1", "LFO 2" })                       g->assignGroup (s, "LFO");
    for (auto* s : { "Bender", "Mod Wheel", "Aftertouch" })    g->assignGroup (s, "MOD");
    g->assignGroup ("Chorus", "CHORUS");
    for (auto* s : { "Voice", "Sequencer" })                   g->assignGroup (s, "VOICE");

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

        if (get ("vca_level") < 35)
            set ("vca_level", 50);

        // An amp envelope that decays to silence needs some decay time to be heard
        auto ampEnv = get ("vca_mode") == env1 ? "env1" : "env2";
        auto sustain = device.getValue (p, device.indexOf (juce::String (ampEnv) + "_sustain"));
        auto decayIndex = device.indexOf (juce::String (ampEnv) + "_decay");

        if (get ("vca_mode") != gate && sustain < 20 && device.getValue (p, decayIndex) < 30)
            device.setValue (p, decayIndex, 30);

        // The filter must open at least a bit (cutoff plus positive envelope amount)
        auto brightness = get ("cutoff") + juce::jmax (0, get ("vcf_env") - centre);

        if (brightness < 35)
            set ("cutoff", 35);
    };

    return g;
}

} // namespace sso::kiwi3p
