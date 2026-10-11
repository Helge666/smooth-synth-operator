# KiwiSix (Korg PolySix upgrade) – MIDI notes from real hardware

Measured on 2026-10-11 against a Korg PolySix with KiwiSix firmware **Prog v5.00 Build 3 /
Boot v3.01** (power-on LEDs: Tone 5, 7, 8), connected directly to a MIDISPORT 4x4.
Reference: *KiwiTechnics KiwiSix Upgrade User Manual v5.0*, MIDI chapter (p. 40–56).

## Firmware update

Firmware, manual and tone sets are in the public folder
<https://www.kiwitechnics.com/downloads/KiwiSix/>. Each update file is **one** large SysEx message
(Intel HEX inside); the bootloader needs it in small packets with pauses (64 bytes, 100 ms worked).
Order: hold Bank B at power-on → boot file → Bank D lights → power-cycle holding Bank B → program
file → Bank D lights → power-cycle. The TAPE switch (SHIFT on the KiwiSix overlay) must be off.

Updating from v3 to v5 converted the stored tones to the new layout automatically and kept the
global settings.

## SysEx

Header `F0 00 21 16 60 02 <dev> <cmd> … F7`, `dev` = Device ID − 1 (default 0).

| Cmd | Function | Verified |
|---|---|---|
| 01 → 02 | request / receive global dump (24 bytes) | yes |
| 03 → 04 | request / receive edit buffer (2 bytes + 128 bytes) | yes |
| 04 | load edit buffer | yes |
| 05 → 06 | request / receive stored tone (bank 0–3, tone 0–127, 128 bytes); loads the tone into the edit buffer | yes |
| 06 | write stored tone — ignored while the front-panel **Write** switch is on Disable | yes |
| 09 / 13 | sequence dumps — **not answered** (v3 and v5) | no reply |
| 0D → 0E | request one edit-buffer parameter (offset) → `0E offset hi lo` | yes |
| 0E | set one edit-buffer parameter: `offset hi lo`. 12-bit values: hi = upper 5 bits, lo = lower 7 bits. Other fields: hi = 0, lo = byte | yes |

The first two bytes of an edit-buffer reply are not always zero (seen: `00 77`); they are ignored.

## Tone data (128 bytes)

Bytes 0–19 ASCII name. 12-bit values are stored as hi (5 bits) / lo (7 bits) byte pairs.

| Offset | Content |
|---|---|
| 14 | VCO octave 0 = 16', 1 = 8', 2 = 4' |
| 15 | VCO wave 0 = Saw, 1 = PW, 2 = PWM |
| 16/17 | PW/PWM amount (12 bit) |
| 18/19 | PWM speed (12 bit) |
| 1A | sub oscillator 0 = off, 1 = −1 oct, 2 = −2 oct |
| 1C/1D | VCF cutoff (12 bit) |
| 1E/1F | VCF resonance (12 bit) |
| 20/21 | VCF EG intensity (12 bit, bipolar, centre 2048) |
| 22/23 | VCF keyboard track (12 bit) |
| 25/26 | VCA level (12 bit) |
| 27 | VCA mode: bit0 = gate (0 = EG) |
| 28–2F | EG attack, decay, sustain, release (12 bit each) |
| 30–35 | MG frequency, delay, level (12 bit each) |
| 36 | MG: bits 0–2 destinations **VCO = 1, VCF = 2, VCA = 4** (manual: reversed); bits 3–5 MG wave (Tri, Sqr, Saw, RevSaw, Random) — not in the manual |
| 37/38 | effect speed/intensity (12 bit) |
| 39 | effect: 0 off, **1 chorus, 2 phase, 4 ensemble** |
| 3A | key assign 0 = Poly 1, 1 = Poly 2, 2 = Unison |
| 3B | arpeggiator range 0–3 (only the low bits are stored) |
| 3C | bit0 = hold |
| 3F | PWM wave (Tri, Sqr, Saw, RevSaw, Random) |
| 40 | arpeggiator clock divide 0–12 |
| 41 | sequencer clock divide 0–12 |
| 42 | unknown; the firmware stores 7 instead of 0 when a tone is written |

The tone set `Kiwi6_factory3.syx` only uses the hi bytes; its lo bytes hold constant filler.

## Control change

CC values set the **upper 7 bits** of 12-bit parameters (lower 5 bits stay). Verified:

| CC | Parameter | CC | Parameter |
|---|---|---|---|
| 7 | VCA level | 47 | EG intensity |
| 8 | octave (0–31, 32–63, 64–127) | 49 | MG level |
| 9 | wave | 50 | MG destination (one at a time) |
| 16 | PW/PWM | 51–54 | attack, decay, sustain, release |
| 19 | MG frequency | 61 | VCA mode: 0–63 gate, 64–127 EG |
| 20 | MG delay | 63 | effect speed/intensity |
| 33 | PWM speed | 65 | MG wave (16 steps per wave) |
| 34 | sub oscillator | 66 | PWM wave |
| 41 | cutoff | 104 | key assign |
| 42 | resonance | 109 | arp range (32 per step, 0–31 = off) |
| 43 | keyboard track | 110 / 111 | arp / sequencer clock divide (10 per step) |
| 118 | effect type: 0–31 off, 32–63 chorus, 64–95 phase, 96–127 ensemble | | |

No effect: CC 108 (arp mode), CC 117. The arp mode is not part of the tone.

NRPN (CC 99 = 0x60, CC 98 = parameter CC number): CC 6 sets the upper 7 bits, CC 38 the lower
5 bits **taken from its upper 5 bits** (value = CC38 >> 2). CC 96/97 step by one.

The app sends 12-bit parameters with SysEx 0E (one message, exact) and switches with CC.

## Writing tones

Read back every written tone: in a run of 512 writes, 5 tones (always the same sound,
"Fat VeloBassII") came back with four wrong values after the first write and were correct after
writing them again.
