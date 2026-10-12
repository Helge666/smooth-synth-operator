namespace sso::hawk800
{

// What the synth and its parameters do, in a few words - for AI control (SynthTools' synth_info).
// Sources: HAWK-800 Owners Manual v2.6.8a, Parameter Quick Reference, our measurements
// (docs/hawk800-midi-notes.md).

juce::String Hawk800Device::getSynthSummary() const
{
    return "VOICES 8 with DCO mode Single, 4 with Double (two DCOs per voice)\n"
           "FLOW DCO 1 (level by EG 1) + DCO 2 (level by EG 2, Double mode only) + noise -> ONE VCF for all voices "
           "(24 dB low pass, cutoff moved by EG 3) -> chorus -> out\n"
           "DCO digital square-wave oscillators; a sound is a mix of footages 16'/8'/4'/2' within the octave. "
           "Square = footages equally loud, Saw = weighted like a sawtooth. Octave 16' + footage 16' = concert pitch\n"
           "SHARED VCF all voices share one filter and EG 3: chords get one filter sweep; with EG3 trigger Single "
           "only the first of held notes restarts it\n"
           "EG Attack time -> Decay time down to the Break Point level -> Slope time to the Sustain level; Release time. "
           "All 0..31, times longer and levels higher with higher values\n"
           "MOD LFO1, LFO2 (rate, delay, waveform) and the slow SLFO3/4 modulate harmonics, DCO pitch (both DCOs alike), "
           "cutoff, resonance and level (tremolo). Modulation waveforms EG3 / Inv EG3 use envelope 3 instead\n"
           "TYPICAL cutoff 0 + EG3 polarity + + EG3 intensity 10-15 = brass/sweep; resonance above 60 gets sharp; "
           "chorus on for width; Double mode + small detune = fat but only 4 voices";
}

void Hawk800Device::addHelp()
{
    setHelp ({
        { "dco1_octave",        "base octave of DCO 1" },
        { "dco1_wave",          "Square: footages equally loud (hollow); Saw: weighted mix (bright, sawtooth-like)" },
        { "dco1_harmonics",     "which footages sound at note start (single footage = plain tone, All = fullest)" },
        { "dco1_hm_wave",       "waveform that switches the footages over time (harmonics modulation)" },
        { "dco1_hm_source",     "LFO that drives the harmonics modulation" },
        { "dco1_hm_depth",      "amount of harmonics modulation, 0 = footages stay as set" },
        { "dco1_level",         "volume of DCO 1 (its envelope is EG 1)" },
        { "dco_mode",           "Single: DCO 1 only, 8 voices; Double: DCO 1 + DCO 2, 4 voices" },
        { "dco2_octave",        "base octave of DCO 2" },
        { "dco2_wave",          "Square: footages equally loud; Saw: weighted mix (like DCO 1)" },
        { "dco2_harmonics",     "which footages of DCO 2 sound at note start" },
        { "dco2_hm_wave",       "waveform of the DCO 2 harmonics modulation" },
        { "dco2_hm_source",     "LFO that drives the DCO 2 harmonics modulation" },
        { "dco2_hm_depth",      "amount of DCO 2 harmonics modulation" },
        { "dco2_level",         "volume of DCO 2 (its envelope is EG 2); only heard in Double mode" },
        { "dco2_interval",      "DCO 2 pitch above DCO 1 in semitones (0 unison, 7 fifth, 12 octave)" },
        { "dco2_detune",        "slight detune of DCO 2 against DCO 1, 0 = none (3 = most)" },
        { "noise",              "noise level into the filter" },
        { "chorus",             "stereo chorus effect" },
        { "cutoff",             "filter cutoff, 0 closed (dark) .. 99 open (bright)" },
        { "resonance",          "filter resonance (emphasis at the cutoff), 0 none .. 99 strong" },
        { "kbd_track",          "cutoff follows the played key (off while VCF velocity is on)" },
        { "eg3_polarity",       "+: EG 3 opens the filter; -: EG 3 closes it" },
        { "eg3_int",            "how much EG 3 moves the cutoff, 0 = not at all" },
        { "eg1_attack",         "EG 1 (DCO 1 volume): attack time" },
        { "eg1_decay",          "EG 1: time from peak down to the break point" },
        { "eg1_break",          "EG 1: level where decay turns into slope" },
        { "eg1_slope",          "EG 1: time from break point to sustain level" },
        { "eg1_sustain",        "EG 1: level while the key is held" },
        { "eg1_release",        "EG 1: fade time after the key is released" },
        { "eg2_attack",         "EG 2 (DCO 2 volume): attack time" },
        { "eg2_decay",          "EG 2: time from peak down to the break point" },
        { "eg2_break",          "EG 2: level where decay turns into slope" },
        { "eg2_slope",          "EG 2: time from break point to sustain level" },
        { "eg2_sustain",        "EG 2: level while the key is held" },
        { "eg2_release",        "EG 2: fade time after the key is released" },
        { "eg3_attack",         "EG 3 (filter and modulation): attack time" },
        { "eg3_decay",          "EG 3: time from peak down to the break point" },
        { "eg3_break",          "EG 3: level where decay turns into slope" },
        { "eg3_slope",          "EG 3: time from break point to sustain level" },
        { "eg3_sustain",        "EG 3: level while keys are held" },
        { "eg3_release",        "EG 3: time after the last key is released" },
        { "eg3_trigger",        "Single: EG 3 starts with the first held note only; Multi: every new note restarts it" },
        { "vel1_int",           "velocity operator 1: how much key velocity changes its target, 0 = off" },
        { "vel1_invert",        "velocity operator 1 reversed (harder playing = less effect)" },
        { "vel1_target",        "envelope parameter changed by velocity operator 1" },
        { "vel2_int",           "velocity operator 2: amount, 0 = off (wins over op 1 on the same target)" },
        { "vel2_invert",        "velocity operator 2 reversed" },
        { "vel2_target",        "envelope parameter changed by velocity operator 2" },
        { "vcf_vel_int",        "how much key velocity opens the filter, 0 = off" },
        { "vcf_vel_shape",      "response curve of velocity to the filter" },
        { "lfo1_rate",          "LFO 1 speed, 0 slowest" },
        { "lfo1_delay",         "time before LFO 1 starts after a note (or stops, with delay invert)" },
        { "lfo1_free",          "off: LFO 1 restarts with each note; on: runs freely" },
        { "lfo1_delay_invert",  "on: LFO 1 runs at note start and stops after the delay" },
        { "lfo1_pw",            "pulse width of the PWM square waveforms of LFO 1 (0 narrow .. 62 square)" },
        { "lfo1_fm_wave",       "SLFO 3 waveform that varies the speed of LFO 1" },
        { "lfo1_fm_depth",      "how much SLFO 3 varies the speed of LFO 1, 0 = steady" },
        { "lfo1_phase",         "where the LFO 1 cycle starts" },
        { "lfo2_rate",          "LFO 2 speed, 0 slowest (see clock sync)" },
        { "lfo2_delay",         "time before LFO 2 starts after a note (or stops, with delay invert)" },
        { "lfo2_free",          "off: LFO 2 restarts with each note; on: runs freely" },
        { "lfo2_delay_invert",  "on: LFO 2 runs at note start and stops after the delay" },
        { "lfo2_pw",            "pulse width of the PWM square waveforms of LFO 2" },
        { "lfo2_fm_wave",       "SLFO 4 waveform that varies the speed of LFO 2" },
        { "lfo2_fm_depth",      "how much SLFO 4 varies the speed of LFO 2, 0 = steady" },
        { "lfo2_sync",          "0: LFO 2 runs on its own; 1-63: one cycle per that many sequencer/MIDI clock ticks" },
        { "slfo3_rate",         "speed of the slow SLFO 3, 0 slowest" },
        { "slfo3_pw",           "pulse width of SLFO 3's PWM square waveforms" },
        { "slfo3_free",         "off: SLFO 3 restarts with each note; on: runs freely" },
        { "rsh_ticks",          "clock ticks between steps of the clocked random sample & hold (sequencer/MIDI clock)" },
        { "slfo4_rate",         "speed of the slow SLFO 4, 0 slowest" },
        { "slfo4_pw",           "pulse width of SLFO 4's PWM square waveforms" },
        { "slfo4_free",         "off: SLFO 4 restarts with each note; on: runs freely" },
        { "slfo4_phase",        "where the SLFO 4 cycle starts" },
        { "dco_lfo_wave",       "waveform of the pitch modulation (vibrato) of both DCOs" },
        { "dco_lfo_source",     "LFO for the pitch modulation" },
        { "dco_lfo_depth",      "vibrato depth, 0 = off" },
        { "dco_eg_invert",      "EG 3 pitch modulation: Add raises the pitch, Subtract lowers it" },
        { "dco_eg_depth",       "how much EG 3 bends the pitch of both DCOs, 0 = off" },
        { "dco_mod_mode",       "pitch modulation range: Normal +-1 semitone; Wide +-7 notes (pitch bend off)" },
        { "vcf_lfo1_wave",      "waveform of the first filter modulation" },
        { "vcf_lfo1_source",    "LFO for the first filter modulation" },
        { "vcf_lfo1_depth",     "first filter modulation amount (wah/growl), 0 = off" },
        { "vcf_lfo2_wave",      "waveform of the second filter modulation" },
        { "vcf_lfo2_source",    "LFO for the second filter modulation" },
        { "vcf_lfo2_depth",     "second filter modulation amount, 0 = off" },
        { "vcf_mod2_mode",      "0: both filter modulations add up; 1-15: the second scales the first (value = minimum)" },
        { "reso_lfo_wave",      "waveform of the resonance modulation" },
        { "reso_lfo_source",    "LFO for the resonance modulation" },
        { "reso_lfo_depth",     "resonance modulation amount, 0 = off" },
        { "reso_eg_depth",      "how much EG 3 moves the resonance, 0 = off" },
        { "reso_eg_invert",     "EG 3 resonance modulation: Add raises it (use low resonance), Subtract lowers it" },
        { "trem1_source",       "tremolo (volume wobble, triangle) on DCO 1: off or the LFO that drives it" },
        { "trem1_depth",        "DCO 1 tremolo depth (deepest with DCO 1 level around 17)" },
        { "trem2_source",       "tremolo on DCO 2: off or the LFO that drives it" },
        { "trem2_depth",        "DCO 2 tremolo depth" },
        { "key_assign",         "Poly; Chord (one key plays a stored chord); Hold; Porta Mono; Porta Unison (all voices on one note)" },
        { "portamento",         "glide in the Porta key modes: 0-7 constant speed (0 fastest), 8-15 constant time (8 fastest)" },
        { "bend_depth",         "joystick pitch bend range, 0 = off" },
        { "pedal_offset",       "extra decay and sustain for EG 1 and 2 while the sustain pedal is held" },
        { "arp_length",         "arpeggiator: most held notes it plays (higher = longer pattern)" },
        { "arp_direction",      "arpeggiator direction" },
        { "arp_range",          "arpeggiator range in octaves" },
        { "arp_sort",           "order of the held notes in the arpeggio" },
        { "arp_played",         "on: the held notes also sound, not just the arpeggio" },
    });
}

} // namespace sso::hawk800
