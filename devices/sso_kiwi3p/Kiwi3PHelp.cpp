namespace sso::kiwi3p
{

// What the synth and its parameters do, in a few words - for AI control (SynthTools' synth_info).
// Sources: KiwiTechnics 3P Upgrade User Manual v400, our measurements (docs/kiwi3p-midi-notes.md).

juce::String Kiwi3PDevice::getSynthSummary() const
{
    return "VOICES 6, two DCOs per voice\n"
           "FLOW DCO 1 + DCO 2 (mix) -> high pass -> VCF (resonant 24 dB low pass) -> VCA -> chorus -> out\n"
           "MOD two ADSR envelopes ENV 1/ENV 2 and two LFOs; each destination picks its source (..._src). "
           "Mod wheel (mw_*), aftertouch (at_*) and the LFO button can also be routed to many destinations\n"
           "DCO 2 can sync to DCO 1 or cross-modulate it (X-Mod Metal) for harder, metallic tones\n"
           "VALUES mostly 0..127; centre-off controls (tune, env depths, mod depths) are 0..127 with 63 = no effect\n"
           "TYPICAL detuned DCOs (dco2_fine away from 63) + chorus = classic wide pad; vcf_env above 63 with "
           "short ENV decay = plucks; Sync + DCO env on DCO 2 = sync sweeps";
}

void Kiwi3PDevice::addHelp()
{
    setHelp ({
        { "dco1_range",     "octave of DCO 1" },
        { "dco1_wave",      "waveform of DCO 1" },
        { "dco1_tune",      "DCO 1 coarse tune, +-1 octave in semitones, 63 = none" },
        { "dco1_lfo",       "LFO pitch modulation of DCO 1 (vibrato), 0 = off" },
        { "dco1_lfo_src",   "LFO for DCO 1's pitch modulation" },
        { "dco1_env",       "envelope pitch modulation of DCO 1, 63 = none, above up, below down" },
        { "dco1_env_src",   "envelope for DCO 1's pitch modulation" },
        { "dco2_range",     "octave of DCO 2" },
        { "dco2_wave",      "waveform of DCO 2 (or noise)" },
        { "dco2_tune",      "DCO 2 coarse tune, +-1 octave in semitones, 63 = none" },
        { "dco2_fine",      "DCO 2 fine tune, +-50 cents, 63 = none; slight offsets thicken the sound" },
        { "dco2_lfo",       "LFO pitch modulation of DCO 2, 0 = off" },
        { "dco2_lfo_src",   "LFO for DCO 2's pitch modulation" },
        { "dco2_env",       "envelope pitch modulation of DCO 2, 63 = none (with Sync: the classic sweep)" },
        { "dco2_env_src",   "envelope for DCO 2's pitch modulation" },
        { "xmod",           "DCO 2 synced to DCO 1 (Sync) or cross-modulated (Metal), or neither" },
        { "dco_mix",        "balance DCO 1 : DCO 2, 0 = only DCO 1, 64 = equal, 127 = only DCO 2" },
        { "env_velocity",   "key velocity scales the envelope pitch modulation of these DCOs" },
        { "hpf",            "high pass cutoff, 0 = full bass .. 127 = thin" },
        { "cutoff",         "low pass cutoff, 0 dark .. 127 open" },
        { "resonance",      "filter resonance (emphasis at the cutoff)" },
        { "vcf_lfo",        "LFO modulation of the cutoff (wah), 0 = off" },
        { "vcf_lfo_src",    "LFO for the cutoff modulation" },
        { "vcf_env",        "envelope modulation of the cutoff, 63 = none, above opens, below closes" },
        { "vcf_env_src",    "envelope for the cutoff modulation" },
        { "key_follow",     "cutoff follows the played key, 0 = not at all" },
        { "vca_mode",       "volume shape: Gate (organ-like on/off) or one of the envelopes" },
        { "vca_level",      "output volume of the sound" },
        { "vca_lfo",        "LFO volume modulation (tremolo), 0 = off" },
        { "vca_lfo_src",    "LFO for the tremolo" },
        { "env1_attack",    "ENV 1 attack time" },
        { "env1_decay",     "ENV 1 decay time down to the sustain level" },
        { "env1_sustain",   "ENV 1 level while the key is held" },
        { "env1_release",   "ENV 1 time after the key is released" },
        { "env2_attack",    "ENV 2 attack time" },
        { "env2_decay",     "ENV 2 decay time down to the sustain level" },
        { "env2_sustain",   "ENV 2 level while the key is held" },
        { "env2_release",   "ENV 2 time after the key is released" },
        { "env_speed",      "Half: both envelopes run at half speed (twice as long)" },
        { "lfo1_wave",      "LFO 1 waveform (Random = sample & hold)" },
        { "lfo1_rate",      "LFO 1 speed, 0 slowest" },
        { "lfo1_delay",     "time before LFO 1 fades in after a note, 0 = at once" },
        { "lfo_mode",       "Normal: LFOs swing above and below; Plus: only above the base value" },
        { "lfo2_wave",      "LFO 2 waveform (Random 2 = random steps clocked by the sequencer)" },
        { "lfo2_rate",      "LFO 2 speed, 0 slowest" },
        { "lfo2_delay",     "time before LFO 2 fades in after a note" },
        { "chorus",         "chorus: off, automatic modulation, or manual (rate below)" },
        { "chorus_rate",    "chorus speed" },
        { "voice_mode",     "Poly 1: 7th note steals a voice; Poly 2: 7th ignored; Unison/Mono: 1 low, 2 high note priority" },
        { "detune",         "detune between the voices (unison/mono, or all modes per global setting)" },
        { "portamento",     "glide time between notes, 0 = off" },
        { "bend_dco",       "pitch bender moves the pitch of these DCOs" },
        { "bend_mix",       "pitch bender moves the DCO mix" },
        { "bend_cutoff",    "pitch bender moves the cutoff" },
        { "bend_lfo",       "pitch bender changes the speed of these LFOs" },
        { "bend_vca",       "pitch bender changes the volume" },
        { "mw_depth",       "mod wheel / LFO button amount, 63 = none, above adds, below subtracts" },
        { "mw_dco_pitch",   "mod wheel shifts the pitch of these DCOs" },
        { "mw_dco_lfo",     "mod wheel adds LFO pitch modulation (vibrato) to these DCOs" },
        { "mw_dco_env",     "mod wheel changes the envelope pitch modulation of these DCOs" },
        { "mw_mix",         "mod wheel moves the DCO mix" },
        { "mw_vcf_lfo",     "mod wheel adds LFO cutoff modulation" },
        { "mw_vcf_env",     "mod wheel changes the envelope cutoff modulation" },
        { "mw_cutoff",      "mod wheel moves the cutoff" },
        { "mw_reso",        "mod wheel moves the resonance" },
        { "mw_hpf",         "mod wheel moves the high pass" },
        { "mw_vca_level",   "mod wheel changes the volume" },
        { "mw_vca_lfo",     "mod wheel adds tremolo" },
        { "mw_lfo_rate",    "mod wheel changes the speed of these LFOs" },
        { "mw_chorus",      "mod wheel changes the chorus rate" },
        { "lfo_button",     "amount the panel's LFO button applies to the mod wheel destinations" },
        { "at_depth",       "aftertouch amount, 63 = none, above adds, below subtracts" },
        { "at_dco_pitch",   "aftertouch shifts the pitch of these DCOs" },
        { "at_dco_lfo",     "aftertouch adds vibrato to these DCOs" },
        { "at_dco_env",     "aftertouch changes the envelope pitch modulation of these DCOs" },
        { "at_mix",         "aftertouch moves the DCO mix" },
        { "at_vcf_lfo",     "aftertouch adds LFO cutoff modulation" },
        { "at_vcf_env",     "aftertouch changes the envelope cutoff modulation" },
        { "at_cutoff",      "aftertouch moves the cutoff" },
        { "at_reso",        "aftertouch moves the resonance" },
        { "at_hpf",         "aftertouch moves the high pass" },
        { "at_vca_level",   "aftertouch changes the volume" },
        { "at_vca_lfo",     "aftertouch adds tremolo" },
        { "at_lfo_rate",    "aftertouch changes the speed of these LFOs" },
        { "at_chorus",      "aftertouch changes the chorus rate" },
        { "load_seq",       "sequence loaded together with this tone, 0 = none" },
        { "load_pattern",   "pattern loaded together with this tone, 0 = none" },
    });
}

} // namespace sso::kiwi3p
