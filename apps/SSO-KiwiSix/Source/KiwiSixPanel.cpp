#include "KiwiSixPanel.h"

namespace theme = sso::theme;
using Def = sso::SectionDef;

namespace
{
    const juce::StringArray waves { "TRI", "SQR", "SAW", "RSW", "RND" };
    const juce::StringArray clocks { "1/2", "1/4", "1/8", "8HS", "8SW", "8T", "1/16", "16HS", "16SW", "16T", "1/32", "32T", "64T" };

    Def knobSection (const char* title, const char* subtitle, float grow, int selectorWidth,
                     std::vector<Def::Fader> knobs, std::vector<Def::Selector> selectors)
    {
        Def d;
        d.title = title;
        d.subtitle = subtitle;
        d.accent = theme::orange;
        d.grow = grow;
        d.selectorWidth = selectorWidth;
        d.faders = std::move (knobs);
        d.selectors = std::move (selectors);
        d.knobs = true;
        return d;
    }

    std::vector<Def> firstRow()
    {
        auto eg = knobSection ("EG", "Envelope", 1.0f, 150,
                               { { "eg_attack", "ATTACK" }, { "eg_decay", "DECAY" }, { "eg_sustain", "SUSTAIN" }, { "eg_release", "RELEASE" } }, {});
        eg.envelope = { "eg_attack", "eg_decay", "eg_sustain", "eg_release" };

        return {
            knobSection ("VCO", "Oscillator + Sub", 1.0f, 200,
                         { { "pw_pwm", "PW / PWM" }, { "pwm_speed", "PWM SPEED" } },
                         { { "vco_octave", "OCTAVE", {} }, { "vco_wave", "WAVEFORM", { "SAW", "PW", "PWM" } },
                           { "pwm_wave", "PWM WAVE", waves }, { "sub_osc", "SUB OSC", { "OFF", "-1 OCT", "-2 OCT" } } }),
            knobSection ("VCF", "24 dB low pass", 1.0f, 0,
                         { { "cutoff", "CUTOFF" }, { "resonance", "RESONANCE" }, { "eg_int", "EG INT" }, { "kbd_track", "KBD TRACK" } }, {}),
            knobSection ("VCA", "", 0.4f, 90,
                         { { "vca_level", "LEVEL" } },
                         { { "vca_mode", "MODE", { "EG", "GATE" } } }),
            eg,
        };
    }

    std::vector<Def> secondRow()
    {
        auto mg = knobSection ("MG", "Modulation generator", 1.0f, 200,
                               { { "mg_rate", "FREQ" }, { "mg_delay", "DELAY" }, { "mg_level", "LEVEL" } },
                               { { "mg_wave", "WAVE", waves } });
        mg.toggles = { { "DESTINATION", { "mg_vco", "mg_vcf", "mg_vca" }, { "VCO", "VCF", "VCA" } } };

        auto keys = knobSection ("Key assign", "Arpeggiator", 1.0f, 280, {},
                                 { { "key_assign", "MODE", { "POLY 1", "POLY 2", "UNISON" } },
                                   { "arp_range", "ARP RANGE", { "OFF", "1 OCT", "2 OCT", "FULL" } },
                                   { "arp_clock", "ARP CLOCK", clocks, 5 } });
        keys.toggles = { { "", { "hold" }, { "HOLD" } } };

        return {
            mg,
            knobSection ("Effects", "", 0.6f, 170,
                         { { "fx_speed", "SPEED / INT" } },
                         { { "fx_type", "MODE", { "OFF", "CHORUS", "PHASE", "ENSEMBLE" }, 2 } }),
            keys,
        };
    }
}

//==============================================================================
KiwiSixPanel::KiwiSixPanel (sso::SynthController& controller, juce::AudioProcessorValueTreeState& state,
                            sso::PatchGenerator& patchGenerator)
{
    for (auto& def : firstRow())
        addAndMakeVisible (*row1.emplace_back (std::make_unique<sso::SectionPanel> (def, controller, state)));

    for (auto& def : secondRow())
        addAndMakeVisible (*row2.emplace_back (std::make_unique<sso::SectionPanel> (def, controller, state)));

    generator = std::make_unique<sso::GeneratorPanel> (patchGenerator, controller);
    addAndMakeVisible (*generator);
}

KiwiSixPanel::~KiwiSixPanel() = default;

void KiwiSixPanel::layoutRow (std::vector<juce::Component*> row, std::vector<float> grows, std::vector<int> preferred,
                              juce::Rectangle<int> area)
{
    constexpr int gap = 12;
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

void KiwiSixPanel::resized()
{
    auto r = getLocalBounds();
    auto rowHeight = (r.getHeight() - 12) / 2;

    auto layoutSections = [] (std::vector<std::unique_ptr<sso::SectionPanel>>& sections, juce::Component* extra,
                              juce::Rectangle<int> area)
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

        if (extra != nullptr)
        {
            comps.push_back (extra);
            grows.push_back (1.0f);
            preferred.push_back (360);
        }

        layoutRow (comps, grows, preferred, area);
    };

    layoutSections (row1, nullptr, r.removeFromTop (rowHeight));
    r.removeFromTop (12);
    layoutSections (row2, generator.get(), r);
}
