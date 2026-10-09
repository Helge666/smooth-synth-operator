# Kiwi-3P (JX-3P upgrade) – MIDI notes from real hardware

Measured on 2026-10-09 against a JX-3P with Kiwi-3P firmware 4.x (power-on LEDs: Tone 4 + 6),
connected directly to a MIDISPORT 4x4 (no other device in the MIDI path).
Reference: *KiwiTechnics 3P Upgrade User Manual v400*, MIDI chapter (p. 39–57).

## SysEx

Header `F0 00 21 16 60 01 <dev> <cmd> … F7`, `dev` = Device ID − 1 (default 0).

| Cmd | Function | Verified |
|---|---|---|
| 01 → 02 | request / receive global dump (48 bytes) | yes |
| 02 | load global dump — **stored permanently**, independent of the memory-protect switch | yes |
| 03 → 04 | request / receive edit buffer (2 null bytes + 128 bytes) | yes |
| 04 | load edit buffer | yes |
| 05 → 06 | request / receive stored tone (bank bit, tone 0–127, 128 bytes). Loads that tone into the edit buffer. | yes |
| 06 | write stored tone — rejected silently with memory protect ON | yes (protect on) |
| 0B | request tone name — **not answered** by this firmware; use 05 instead | no reply |
| 0E | set one edit-buffer byte (offset, value) | yes |
| 10 | set one global byte (offset, value) — **RAM only**, lost at power-off | yes |

A response takes ~60 ms (139 bytes at 31.25 kBaud plus processing).

## Tone data (128 bytes)

Layout as in the manual (bytes 0–19 ASCII name). Deviations / additions found:

- `0x18`, `0x1E`, `0x28`, `0x3F` ("not used") hold `0x3F` in real patches – preserved.
- `0x20` DCO mix is stored **inverted** relative to CC 17 (CC 0 → stored 127).
- `0x33`/`0x36` LFO wave: stored order Sine 0, Tri 1, **Square 2**, Saw 3, Rev Saw 4, Random 5,
  Random 2 6. A factory-area patch ("Space Wheel", C-2) holds the undocumented value 7 for LFO 2.
- `0x34`/`0x37` LFO rate: CC values below 4 are stored as 4, but patches can contain 0–3.
- `0x15`/`0x1A`/`0x1B` tune/fine: CC 127 is stored as 126; factory patches contain 127.
- `0x3B` chorus flags: bit0 on, bit1 auto → Off 0, Auto 3, Manual 1.
- `0x3E` VCA flags: bit0 gate (wins), bit1 ENV 2, bit2 LFO 2.
- `0x54` envelope speed: CC 69 is accepted but does not change the stored byte → use SysEx 0E.

## Control change

All CCs in the manual's table were verified to land in the expected byte, with these corrections:

- **CC 25 DCO 2 wave**: order is Saw, Pulse, Square, Noise (0–31, 32–63, 64–95, 96–127);
  the manual lists Noise first.
- CC 7 sets VCA level (0x3C) only.
- No CC exists for mod-wheel / aftertouch routing (0x45–0x4F), DCO env velocity (0x21 bits 4–5)
  and envelope speed (0x54) → SysEx 0E.

## Pitfalls

- **Never create a MIDI loop.** With a Kiwi Patch Editor 6.4 in the path (thru both ways), the
  synth's own dump replies came back merged with CCs and were loaded as garbage – including
  a permanently stored global dump. Remove such devices or make sure the synth's output never
  reaches its input.
- After an edit-buffer dump the DCO footage can keep its previous setting until it is sent
  again; the app re-sends DCO range/wave after every dump.
- Loading a stored tone (05) replaces the edit buffer.
