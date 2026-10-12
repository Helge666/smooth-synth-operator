#include "Hawk800Panel.h"

namespace theme = sso::theme;
using Def = sso::SectionDef;

namespace
{
    constexpr int knob = 52;    // smaller knobs than the PolySix: the Poly-800 has many more parameters

    const juce::StringArray footages { "16'", "8'", "4'", "2'" };
    // Harmonics selector value -> footages on (bit 0 = 16' ... bit 3 = 2'), manual order
    const std::vector<int> footageFlags { 1, 2, 4, 8, 3, 5, 9, 6, 10, 12, 7, 11, 13, 14, 15, 0 };
    const juce::StringArray sources { "LFO1", "LFO2", "SLFO3", "SLFO4" };

    Def section (const char* title, const char* subtitle, float grow, int selectorWidth, int knobColumns,
                 std::vector<Def::Fader> knobs, std::vector<Def::Selector> selectors = {},
                 std::vector<Def::Toggles> toggles = {})
    {
        Def d;
        d.title = title;
        d.subtitle = subtitle;
        d.accent = theme::accent;
        d.grow = grow;
        d.selectorWidth = selectorWidth;
        d.faders = std::move (knobs);
        d.selectors = std::move (selectors);
        d.toggles = std::move (toggles);
        d.knobs = true;
        d.knobSize = knob;
        d.knobColumns = knobColumns;
        return d;
    }

    Def envelope (const char* title, const char* subtitle, const char* prefix, std::vector<Def::Selector> selectors = {})
    {
        auto id = [prefix] (const char* s) { return juce::String (prefix) + s; };
        auto d = section (title, subtitle, 1.0f, 150, 3,
                          { { id ("_attack"), "ATTACK" }, { id ("_decay"), "DECAY" }, { id ("_break"), "BREAK" },
                            { id ("_slope"), "SLOPE" }, { id ("_sustain"), "SUSTAIN" }, { id ("_release"), "RELEASE" } },
                          std::move (selectors));
        d.envelope = { id ("_attack"), id ("_decay"), id ("_break"), id ("_slope"), id ("_sustain"), id ("_release") };
        d.graphHeight = 70;
        return d;
    }

    std::vector<Def> firstRow()
    {
        auto dco = [] (const char* title, const char* subtitle, const char* p, std::vector<Def::Fader> extraKnobs,
                       std::vector<Def::Selector> extraSelectors)
        {
            auto id = [p] (const char* s) { return juce::String (p) + s; };
            std::vector<Def::Fader> knobs { { id ("_level"), "LEVEL" }, { id ("_hm_depth"), "HARM MOD" },
                                            { id ("_hm_wave"), "HM WAVE" }, { id ("_hm_source"), "HM SOURCE" } };
            knobs.insert (knobs.end(), extraKnobs.begin(), extraKnobs.end());
            std::vector<Def::Selector> selectors { { id ("_octave"), "OCTAVE", {} }, { id ("_wave"), "WAVEFORM", { "SQUARE", "SAW" } },
                                                   { id ("_harmonics"), "FOOTAGES", footages, 0, footageFlags } };
            selectors.insert (selectors.end(), extraSelectors.begin(), extraSelectors.end());
            return section (title, subtitle, 1.0f, 190, extraKnobs.empty() ? 2 : 3, knobs, selectors);
        };

        return {
            dco ("DCO 1", "Level from EG 1", "dco1", {}, { { "dco_mode", "DCO MODE", { "SINGLE", "DOUBLE" } } }),
            dco ("DCO 2", "Level from EG 2, double mode only", "dco2", { { "dco2_interval", "INTERVAL" }, { "dco2_detune", "DETUNE" } }, {}),
            section ("Noise", "", 0.3f, 90, 1, { { "noise", "LEVEL" } }, { { "chorus", "CHORUS", { "OFF", "ON" } } }),
            section ("VCF", "Low pass", 0.8f, 150, 3,
                     { { "cutoff", "CUTOFF" }, { "resonance", "RESONANCE" }, { "eg3_int", "EG3 INT" } },
                     { { "kbd_track", "KBD TRACK", { "OFF", "HALF", "FULL" } }, { "eg3_polarity", "EG3 POLARITY", { "-", "+" } } }),
        };
    }

    std::vector<Def> secondRow()
    {
        return {
            envelope ("EG 1", "DCO 1 amp", "eg1"),
            envelope ("EG 2", "DCO 2 amp", "eg2"),
            envelope ("EG 3", "VCF", "eg3", { { "eg3_trigger", "TRIGGER", { "SINGLE", "MULTI" } } }),
            section ("Velocity", "", 0.6f, 0, 3,
                     { { "vel1_int", "OP1 INT" }, { "vel1_target", "OP1 TARGET" }, { "vcf_vel_int", "VCF INT" },
                       { "vel2_int", "OP2 INT" }, { "vel2_target", "OP2 TARGET" }, { "vcf_vel_shape", "VCF SHAPE" } },
                     {}, {}),
        };
    }

    /** The HAWK's extra pages, shown one at a time in the third row. */
    std::vector<std::pair<juce::String, std::vector<Def>>> modulationPages()
    {
        auto lfo = [] (const char* title, const char* p, Def::Fader extra)
        {
            auto id = [p] (const char* s) { return juce::String (p) + s; };
            return section (title, "", 1.0f, 170, 0,
                            { { id ("_rate"), "RATE" }, { id ("_delay"), "DELAY" }, { id ("_pw"), "PW PHASE" },
                              { id ("_fm_wave"), "RATE MOD" }, { id ("_fm_depth"), "RM DEPTH" }, extra },
                            {}, { { "", { id ("_free"), id ("_delay_invert") }, { "FREE RUN", "DELAY INV" } } });
        };

        return {
            { "LFO 1+2", { lfo ("LFO 1", "lfo1", { "lfo1_phase", "START" }), lfo ("LFO 2", "lfo2", { "lfo2_sync", "CLOCK SYNC" }) } },
            { "SLFO 3+4", {
                section ("SLFO 3", "Special LFO", 1.0f, 150, 0,
                         { { "slfo3_rate", "RATE" }, { "slfo3_pw", "PW PHASE" }, { "rsh_ticks", "S&H TICKS" } },
                         {}, { { "", { "slfo3_free" }, { "FREE RUN" } } }),
                section ("SLFO 4", "Special LFO", 1.0f, 150, 0,
                         { { "slfo4_rate", "RATE" }, { "slfo4_pw", "PW PHASE" }, { "slfo4_phase", "START" } },
                         {}, { { "", { "slfo4_free" }, { "FREE RUN" } } }) } },
            { "DCO MOD", {
                section ("DCO pitch", "LFO and EG 3", 1.2f, 220, 0,
                         { { "dco_lfo_wave", "LFO WAVE" }, { "dco_lfo_depth", "LFO DEPTH" }, { "dco_eg_depth", "EG3 DEPTH" } },
                         { { "dco_lfo_source", "LFO SOURCE", sources }, { "dco_eg_invert", "EG3 MODE", { "ADD", "SUB" } } }),
                section ("Range", "", 0.4f, 170, 0, {},
                         { { "dco_mod_mode", "MOD RANGE", { "NORMAL", "WIDE", "NOTE" } } }),
                section ("Tremolo", "", 1.0f, 150, 0,
                         { { "trem1_depth", "DCO1 DEPTH" }, { "trem2_depth", "DCO2 DEPTH" } },
                         { { "trem1_source", "DCO1", { "OFF", "LFO1", "LFO2" } }, { "trem2_source", "DCO2", { "OFF", "LFO1", "LFO2" } } }) } },
            { "VCF MOD", {
                section ("VCF mod 1", "", 1.0f, 200, 0,
                         { { "vcf_lfo1_wave", "WAVE" }, { "vcf_lfo1_depth", "DEPTH" } },
                         { { "vcf_lfo1_source", "SOURCE", sources } }),
                section ("VCF mod 2", "", 1.0f, 200, 0,
                         { { "vcf_lfo2_wave", "WAVE" }, { "vcf_lfo2_depth", "DEPTH" }, { "vcf_mod2_mode", "ATTENUATE" } },
                         { { "vcf_lfo2_source", "SOURCE", sources } }),
                section ("Resonance mod", "", 1.0f, 200, 0,
                         { { "reso_lfo_wave", "LFO WAVE" }, { "reso_lfo_depth", "LFO DEPTH" }, { "reso_eg_depth", "EG3 DEPTH" } },
                         { { "reso_lfo_source", "LFO SOURCE", sources }, { "reso_eg_invert", "EG3 MODE", { "ADD", "SUB" } } }) } },
            { "PLAY + ARP", {
                section ("Play", "Key assign", 1.2f, 330, 0,
                         { { "portamento", "PORTA" }, { "bend_depth", "BEND" }, { "pedal_offset", "PEDAL" } },
                         { { "key_assign", "KEY ASSIGN", { "POLY", "CHORD", "HOLD", "MONO", "UNISON" } } }),
                section ("Arpeggiator", "Bank Hold + Start", 1.0f, 260, 0,
                         { { "arp_length", "LENGTH" } },
                         { { "arp_direction", "DIRECTION", { "UP", "DOWN", "U/D" } }, { "arp_range", "RANGE", { "1 OCT", "2 OCT", "3 OCT" } },
                           { "arp_sort", "SORT", { "NONE", "DOWN", "U/D" } } },
                         { { "", { "arp_played" }, { "SOUND PLAYED" } } }) } },
        };
    }

    void layoutRow (std::vector<juce::Component*> row, std::vector<float> grows, std::vector<int> preferred,
                    juce::Rectangle<int> area)
    {
        constexpr int gap = 10;
        auto available = area.getWidth() - (int) (row.size() - 1) * gap;
        auto total = 0;
        float growSum = 0;

        for (size_t i = 0; i < row.size(); ++i)
        {
            total += preferred[i];
            growSum += grows[i];
        }

        // Too narrow: shrink everything evenly. Wider: share the rest by `grows`.
        auto shrink = total > available ? (float) available / (float) total : 1.0f;
        auto extra = (float) juce::jmax (0, available - total);
        auto x = (float) area.getX();

        for (size_t i = 0; i < row.size(); ++i)
        {
            auto w = (float) preferred[i] * shrink + (growSum > 0 ? extra * grows[i] / growSum : 0.0f);
            row[i]->setBounds (juce::Rectangle<float> (x, (float) area.getY(), w, (float) area.getHeight()).toNearestInt());
            x += w + (float) gap;
        }
    }

    void layoutSections (std::vector<std::unique_ptr<sso::SectionPanel>>& sections, juce::Rectangle<int> area)
    {
        std::vector<juce::Component*> comps;
        std::vector<float> grows;
        std::vector<int> preferred;

        for (auto& s : sections)
        {
            comps.push_back (s.get());
            grows.push_back (s->getDef().grow);
            preferred.push_back (s->getPreferredWidth());
        }

        layoutRow (comps, grows, preferred, area);
    }
}

//==============================================================================
Hawk800Panel::Hawk800Panel (sso::SynthController& controller, juce::AudioProcessorValueTreeState& state,
                            sso::PatchGenerator& patchGenerator)
{
    for (auto& def : firstRow())
        addAndMakeVisible (*row1.emplace_back (std::make_unique<sso::SectionPanel> (def, controller, state)));

    for (auto& def : secondRow())
        addAndMakeVisible (*row2.emplace_back (std::make_unique<sso::SectionPanel> (def, controller, state)));

    for (auto& [name, defs] : modulationPages())
    {
        auto& page = pages.emplace_back();

        for (auto& def : defs)
            addChildComponent (*page.emplace_back (std::make_unique<sso::SectionPanel> (def, controller, state)));

        auto* b = pageButtons.add (new juce::TextButton (name));
        b->setClickingTogglesState (false);
        b->onClick = [this, index = pageButtons.size() - 1] { showPage (index); };
        addAndMakeVisible (b);
    }

    generator = std::make_unique<sso::GeneratorPanel> (patchGenerator, controller);
    addAndMakeVisible (*generator);

    showPage (settings->get().getIntValue ("hawk800.modPage", 0));
}

Hawk800Panel::~Hawk800Panel() = default;

void Hawk800Panel::showPage (int index)
{
    currentPage = juce::jlimit (0, (int) pages.size() - 1, index);
    settings->get().setValue ("hawk800.modPage", currentPage);

    for (size_t p = 0; p < pages.size(); ++p)
        for (auto& s : pages[p])
            s->setVisible ((int) p == currentPage);

    for (int i = 0; i < pageButtons.size(); ++i)
    {
        auto on = i == currentPage;
        pageButtons[i]->setColour (juce::TextButton::buttonColourId, on ? theme::tabFill : theme::control);
        pageButtons[i]->setColour (juce::TextButton::textColourOffId, on ? theme::tabText : theme::text);
    }

    resized();
}

void Hawk800Panel::resized()
{
    constexpr int gap = 10;
    auto r = getLocalBounds();
    auto rowHeight = (r.getHeight() - 2 * gap) / 3;

    layoutSections (row1, r.removeFromTop (rowHeight));
    r.removeFromTop (gap);
    layoutSections (row2, r.removeFromTop (rowHeight));
    r.removeFromTop (gap);

    generator->setBounds (r.removeFromRight (360));
    r.removeFromRight (gap);

    auto tabs = r.removeFromTop (pageTabHeight);

    for (auto* b : pageButtons)
    {
        b->setBounds (tabs.removeFromLeft (120).withTrimmedRight (4));
    }

    r.removeFromTop (6);

    if (juce::isPositiveAndBelow (currentPage, (int) pages.size()))
        layoutSections (pages[(size_t) currentPage], r);
}
