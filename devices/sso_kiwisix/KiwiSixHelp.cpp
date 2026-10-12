namespace sso::kiwisix
{

// What the synth and its parameters do, in a few words - for AI control (SynthTools' synth_info).
// Sources: KiwiSix Upgrade User Manual v5.0, our measurements (docs/kiwisix-midi-notes.md).

juce::String KiwiSixDevice::getSynthSummary() const
{
    return "VOICES 6, one VCO plus sub oscillator per voice (analog; the voices differ slightly)\n"
           "FLOW VCO (+ sub) -> VCF (resonant 24 dB low pass, self-oscillates near full resonance) -> VCA -> "
           "effects (chorus/phase/ensemble) -> out\n"
           "MOD one ADSR envelope (EG) for the filter (eg_int) and the volume (VCA mode EG); one MG (LFO) to VCO, "
           "VCF and/or VCA; a separate LFO for pulse width modulation\n"
           "VALUES 12 bit: 0..4095, 2048 = half way; centre-off controls have 2048 = no effect\n"
           "TYPICAL PWM + ensemble = lush strings; cutoff low + eg_int high + short decay = plucks/bass; "
           "sub osc 1 Oct Down for weight";
}

void KiwiSixDevice::addHelp()
{
    setHelp ({
        { "vco_octave",   "octave of the VCO" },
        { "vco_wave",     "Saw; PW = pulse with fixed width; PWM = pulse width moved by the PWM LFO" },
        { "pw_pwm",       "pulse width (PW) or depth of the pulse width modulation (PWM)" },
        { "pwm_speed",    "speed of the pulse width modulation" },
        { "pwm_wave",     "waveform of the pulse width modulation" },
        { "sub_osc",      "square sub oscillator one or two octaves below the VCO" },
        { "cutoff",       "filter cutoff, 0 dark .. 4095 open" },
        { "resonance",    "filter resonance; whistles (self-oscillates) near the top" },
        { "eg_int",       "envelope modulation of the cutoff, 2048 = none, above opens, below closes" },
        { "kbd_track",    "cutoff follows the played key; 2048 = neutral, above follows, below goes the other way" },
        { "vca_level",    "output volume of the sound" },
        { "vca_mode",     "EG: the envelope shapes the volume; Gate: organ-like on/off" },
        { "eg_attack",    "envelope attack time" },
        { "eg_decay",     "envelope decay time down to the sustain level" },
        { "eg_sustain",   "envelope level while the key is held" },
        { "eg_release",   "envelope time after the key is released" },
        { "mg_rate",      "MG (LFO) speed" },
        { "mg_delay",     "time before the MG fades in after a note" },
        { "mg_level",     "MG depth for the destinations switched on below" },
        { "mg_vco",       "MG modulates the pitch (vibrato)" },
        { "mg_vcf",       "MG modulates the cutoff (wah)" },
        { "mg_vca",       "MG modulates the volume (tremolo)" },
        { "mg_wave",      "MG waveform (Random = sample & hold)" },
        { "fx_type",      "effect: chorus, phaser or ensemble (richest), or off" },
        { "fx_speed",     "effect speed / intensity" },
        { "key_assign",   "Poly 1 / Poly 2: polyphonic voice assignment variants; Unison: all voices on one note (fat mono)" },
        { "hold",         "held notes keep sounding after the keys are released" },
        { "arp_range",    "arpeggiator: off, or its range" },
        { "arp_clock",    "arpeggiator step length (clock divider)" },
        { "seq_clock",    "sequencer step length (clock divider)" },
    });
}

} // namespace sso::kiwisix
