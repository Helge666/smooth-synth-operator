#include "Kiwi3PPanel.h"

namespace theme = sso::theme;
using Def = sso::SectionDef;

namespace
{
    const juce::StringArray sourcesLfo { "L1", "L2" }, sourcesEnv { "E1", "E2" };
    const juce::StringArray lfo1Waves { "SIN", "TRI", "SAW", "RSW", "SQR", "RND", "W7", "W8" };
    const juce::StringArray lfo2Waves { "SIN", "TRI", "SAW", "RSW", "SQR", "RND", "RN2", "W8" };

    std::vector<Def> firstRow()
    {
        return {
            { "DCO-1", theme::orange, 1.0f, 120,
              { { "dco1_tune", "TUNE" }, { "dco1_lfo", "LFO" }, { "dco1_env", "ENV" } },
              { { "dco1_range", "RANGE", {} }, { "dco1_wave", "WAVE", { "SAW", "PLS", "SQR" } },
                { "dco1_lfo_src", "LFO SOURCE", sourcesLfo }, { "dco1_env_src", "ENV SOURCE", sourcesEnv } } },

            { "DCO-2", theme::orange, 1.0f, 160,
              { { "dco2_tune", "TUNE" }, { "dco2_fine", "FINE" }, { "dco2_lfo", "LFO" }, { "dco2_env", "ENV" } },
              { { "dco2_range", "RANGE", {} }, { "dco2_wave", "WAVE", { "SAW", "PLS", "SQR", "NOI" } },
                { "xmod", "X-MOD", { "OFF", "SYNC", "METAL" } },
                { "dco2_lfo_src", "LFO SOURCE", sourcesLfo }, { "dco2_env_src", "ENV SOURCE", sourcesEnv } } },

            { "Mixer", theme::cream, 0.5f, 80,
              { { "dco_mix", "1 : 2" } },
              { { "env_velocity", "ENV VELO", { "OFF", "1", "2", "1+2" } } } },

            { "VCF", theme::red, 1.0f, 80,
              { { "hpf", "HPF" }, { "cutoff", "CUTOFF" }, { "resonance", "RES" }, { "vcf_lfo", "LFO" },
                { "vcf_env", "ENV" }, { "key_follow", "KEY" } },
              { { "vcf_lfo_src", "LFO SOURCE", sourcesLfo }, { "vcf_env_src", "ENV SOURCE", sourcesEnv } } },

            { "VCA", theme::red, 0.7f, 150,
              { { "vca_level", "LEVEL" }, { "vca_lfo", "LFO" } },
              { { "vca_mode", "MODE", { "GATE", "ENV1", "ENV2" } }, { "vca_lfo_src", "LFO SOURCE", sourcesLfo } } },
        };
    }

    std::vector<Def> secondRow()
    {
        Def env1 { "ENV-1", theme::yellow, 1.0f, 150,
                   { { "env1_attack", "A" }, { "env1_decay", "D" }, { "env1_sustain", "S" }, { "env1_release", "R" } }, {} };
        env1.envelope = { "env1_attack", "env1_decay", "env1_sustain", "env1_release" };

        Def env2 { "ENV-2", theme::yellow, 1.0f, 150,
                   { { "env2_attack", "A" }, { "env2_decay", "D" }, { "env2_sustain", "S" }, { "env2_release", "R" } },
                   { { "env_speed", "SPEED 1+2", { "NORM", "HALF" } } } };
        env2.envelope = { "env2_attack", "env2_decay", "env2_sustain", "env2_release" };

        return {
            env1,
            env2,
            { "LFO-1", theme::blue, 0.7f, 120,
              { { "lfo1_rate", "RATE" }, { "lfo1_delay", "DELAY" } },
              { { "lfo1_wave", "WAVE", lfo1Waves }, { "lfo_mode", "MODE 1+2", { "NORM", "PLUS" } } } },
            { "LFO-2", theme::blue, 0.7f, 120,
              { { "lfo2_rate", "RATE" }, { "lfo2_delay", "DELAY" } },
              { { "lfo2_wave", "WAVE", lfo2Waves } } },
            { "Chorus", theme::blue, 0.5f, 150,
              { { "chorus_rate", "RATE" } },
              { { "chorus", "MODE", { "OFF", "AUTO", "MAN" } } } },
        };
    }

    sso::ModMatrixDef matrixDef()
    {
        // destinations:     PITCH        D-LFO        D-ENV        MIX          F-LFO          F-ENV          CUT             RES          HPF          VCA              A-LFO          LFO RT         CHOR
        auto routing = [] (const juce::String& p) -> std::vector<juce::String>
        {
            return { p + "_dco_pitch", p + "_dco_lfo", p + "_dco_env", p + "_mix", p + "_vcf_lfo", p + "_vcf_env",
                     p + "_cutoff", p + "_reso", p + "_hpf", p + "_vca_level", p + "_vca_lfo", p + "_lfo_rate", p + "_chorus" };
        };

        sso::ModMatrixDef def;
        def.title = "Modulation";
        def.accent = theme::blue;
        def.destinations = { "PITCH", "D-LFO", "D-ENV", "MIX", "F-LFO", "F-ENV", "CUT", "RES", "HPF", "VCA", "A-LFO", "RATE", "CHOR" };
        def.rows = {
            { "BENDER", {}, { "bend_dco", "", "", "bend_mix", "", "", "bend_cutoff", "", "", "bend_vca", "", "bend_lfo", "" } },
            { "WHEEL/LFO", "mw_depth", routing ("mw") },
            { "AFTERTOUCH", "at_depth", routing ("at") },
        };
        return def;
    }
}

//==============================================================================
Kiwi3PPanel::Kiwi3PPanel (sso::SynthController& controller, juce::AudioProcessorValueTreeState& state,
                          sso::PatchGenerator& patchGenerator)
{
    for (auto& def : firstRow())
        addAndMakeVisible (*row1.emplace_back (std::make_unique<sso::SectionPanel> (def, controller, state)));

    for (auto& def : secondRow())
        addAndMakeVisible (*row2.emplace_back (std::make_unique<sso::SectionPanel> (def, controller, state)));

    matrix = std::make_unique<sso::ModMatrix> (matrixDef(), controller, state);
    addAndMakeVisible (*matrix);

    Def voiceDef { "Voice", theme::cream, 1.0f, 150,
                   { { "detune", "DETUNE" }, { "portamento", "PORTA" }, { "load_seq", "SEQ" }, { "load_pattern", "PATT" } },
                   { { "voice_mode", "ASSIGN", { "POLY1", "POLY2", "UNI1", "UNI2", "MONO1", "MONO2" } } } };
    voice = std::make_unique<sso::SectionPanel> (voiceDef, controller, state);
    addAndMakeVisible (*voice);

    generator = std::make_unique<sso::GeneratorPanel> (patchGenerator, controller);
    addAndMakeVisible (*generator);
}

Kiwi3PPanel::~Kiwi3PPanel() = default;

void Kiwi3PPanel::layoutRow (std::vector<juce::Component*> row, std::vector<float> grows, std::vector<int> preferred,
                             juce::Rectangle<int> area)
{
    constexpr int gap = 12;
    auto total = (int) (row.size() - 1) * gap;
    float growSum = 0;

    for (size_t i = 0; i < row.size(); ++i)
    {
        total += preferred[i];
        growSum += grows[i];
    }

    auto extra = (float) juce::jmax (0, area.getWidth() - total);
    auto x = (float) area.getX();

    for (size_t i = 0; i < row.size(); ++i)
    {
        auto w = (float) preferred[i] + extra * grows[i] / growSum;
        row[i]->setBounds (juce::Rectangle<float> (x, (float) area.getY(), w, (float) area.getHeight()).toNearestInt());
        x += w + (float) gap;
    }
}

void Kiwi3PPanel::resized()
{
    auto r = getLocalBounds();

    auto layoutSections = [this] (std::vector<std::unique_ptr<sso::SectionPanel>>& sections, juce::Rectangle<int> area)
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
    };

    layoutSections (row1, r.removeFromTop (rowHeight));
    r.removeFromTop (12);
    layoutSections (row2, r.removeFromTop (rowHeight));
    r.removeFromTop (12);

    layoutRow ({ matrix.get(), voice.get(), generator.get() }, { 1.0f, 0.0f, 0.0f },
               { 600, voice->getPreferredWidth(), 380 }, r);
}
