# HAWK-800 (Korg Poly-800 upgrade) – MIDI notes from real hardware

Measured on 2026-10-12 against a Korg Poly-800 MK1 with HAWK-800 software **v2.68b** (a bug-fix
build of v2.68a; same parameters), no AtomaHawk board, connected directly to a MIDISPORT 4x4.
Reference: *HAWK-800 Owners Manual v2.6.8a*, *MIDI Implementation Chart v2.6.8* and
*Parameter Quick Reference v2.6.8a* (all in `Version-2.68a.zip`).

## Firmware

Software, documentation and release notes: <https://www.hawk800.com/techsupport.php>
(`download/Packages/Version-2.68a.zip`); older versions on
<https://sourceforge.net/projects/hawk800/files/>. v2.68 is the last version for the HAWK-800 –
its program memory is full; the successor is a different kit (HAWK-832).

Flash update (manual p. 39): hold **Write** while powering on → display "FLASH"; send
`EEPROM0.SYX` → "Good 0", press Write; `EEPROM1.SYX` → "Good 1", Write; `EEPROM2.SYX` →
"Good 2"; Prog/Para restarts. Each file is one 16 KB SysEx (`F0 42 21 7F 00 0n …`). Do not
flash `BOOT.SYX` unless support says so.

Installed version: power-on display "P1 5E XX", or Tools mode (hold Prog/Para at power-on) → 1.

## SysEx

Header `F0 42 21 <cmd> … F7` (Korg, Poly-800). No channel or device ID. Data bytes are sent as
high/low nibble pairs.

| Cmd | Function | Verified |
|---|---|---|
| 0D → 7E 0D x | identify: x = 0 Poly-800 MK1, 1 EX-800, 2 MK2 | yes (0) |
| 07 x → 06 x | request patch x (0–63 = patches 11–88 of the current bank; **64 = edit buffer**) | yes |
| 06 x data | receive patch: x 0–63 stores into the current bank, 64 loads the edit buffer; ack `7E 06 x` | edit buffer: yes |
| 0A 00 / 01 / 02 | bulk dump request: all 64 patches of the bank / sequencer data / globals | 01, 02: yes |
| 08 data | globals (64 bytes as nibbles); no ack | reply format: yes |
| 0B data | sequencer data (256 bytes as **low/high** nibbles), ack `7E 0B 01` | reply format: yes |
| 0E x | change patch bank (x = 0–3), ack `7E 0E 01`; changes global 13 | yes |
| 0F pp vv | set edit-buffer parameter pp (0–63 = P1 11–88, 64–127 = P2 11–88) | yes |
| 12 | write the running patch into the selected patch; `7E 12 00` if the write switch is off | no |
| 02–05 | peek / poke / dump / erase memory – never used | – |

**Processing time:** an edit-buffer dump (`06 40`) is acknowledged about 0.4 s after it arrived.
A note sent meanwhile is lost (only a click is heard); SSO waits a whole second after a dump
before it plays notes (`getSettleTimeMs`). Read-only requests (`0A 02`, `0D`) and CC 120 do not
disturb held notes (measured 2026-10-12).

## Patch data (128 bytes)

Byte *i* = parameter *i*: bytes 0–63 are P1 11, 12 … 18, 21 … 88 (original Poly-800 parameters),
bytes 64–127 are P2 11 … 88 (HAWK extended parameters). Unused numbers (P1 28, 78; P2 34, 37,
55, 65) stay 0.

Values are **stored 0-based**: parameters that the manual lists from 1 (e.g. octave 1–3,
LFO waveform 1–13, arpeggiator length 3–16) are stored as manual value − 1. `0F` takes the
stored value directly and clamps it at the maximum (verified for every parameter: each message
changed exactly its own byte, nothing else).

Measured meanings that differ from or add to the manual (audio recorded from the synth):

- **P1 12/22 waveform**: stored 0 = square (all footages equally loud), 1 = sawtooth (footages
  weighted; harmonics at -6, -9.5, -12, -14 dB). With a single footage both sound the same.
- **P1 13/23 harmonics** (footages within the octave): stored 0-15 = 16', 8', 4', 2', 16'+8',
  16'+4', 16'+2', 8'+4', 8'+2', 4'+2', 16'+8'+4', 16'+8'+2', 16'+4'+2', 8'+4'+2', all, none - as
  in the manual. Octave 16' with footage 16' is concert pitch: note 57 plays 220 Hz, note 69
  440 Hz (octave 8' plays 880 Hz). SSO's TUNE sound uses this with a single footage (square).
- **P1 43 EG3 polarity**: stored **1 = positive** (EG3 opens the filter), 0 = negative - the
  manual's "1 = positive" is the reverse. The Korg brass sounds use cutoff 0 with polarity 1.
- **P1 31 interval** goes up to 12 (octave) in the factory sounds, the manual says 0-11.

## Factory banks

<https://www.hawk800.com/download/Patches/> `Bank1.syx` … `Bank4.syx` (same files on
SourceForge "Patch Library"). Each is one SysEx for tools mode 8: `F0 42 21 7F 00 <04-07 = bank
1-4>`, then as nibbles a checksum byte (sum of the data bytes, low 8 bits) and 64 patches of 128
bytes. (`00 00`-`00 03` are the software and boot images.) No names anywhere.

- Bank 2 = the original Korg Poly-800 programs 11-88 (with HAWK resonance values added); the
  names come from the Korg owner's manual. Checked by ear: 12 Acoustic Piano, 38 Helicopter,
  68 Steel Drums, 88 UFO Swoop.
- Banks 1 and 4 are mostly HAWK variations of the same programs (same P1 values, extra
  modulation); bank 4 is largely a copy of bank 1. Bank 3 is a different set.
- SSO names the library copies: Korg names for bank 2, "<name> A/B/..." for sounds with the same
  P1 values, "Sound <bank>-<number>" for the rest; exact copies are left out (201 sounds).

## Globals (64 bytes)

Byte *i* = global *i* in the same 11–88 numbering. Global 13 (patch bank) is stored 0-based
(0 = bank 1). Global 47 = 1 (absolute CC/parameter values) on this unit.

## Write protection

The rear panel has two switches, **PROGRAM WRITE** and **SEQUENCER WRITE** (enable/disable).
On this unit both were on *disable* during the measurements above.

## Banks

4 banks × 64 patches (11–88). Program change 0–63 selects a patch, CC 32 selects the bank.
