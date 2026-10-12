# Smooth Synth Operator

Modern software programmers for vintage synthesizers with modern upgrades —
standalone app and VST3 plugin.

Programmers:

- **SSO Kiwi-3P** for the Roland JX-3P with the KiwiTechnics Kiwi-3P upgrade, inspired by the Retroaktiv PG-2K
- **SSO KiwiSix** for the Korg PolySix with the KiwiTechnics KiwiSix upgrade (firmware v5.00) -
  12-bit knobs, 512 memories in banks A-H, PolySix look
- **SSO Hawk-800** for the Korg Poly-800 with the HAWK-800 upgrade (software v2.68) - all 64
  original and 64 extended parameters (minus MK2 effects and AtomaHawk-only ones), 4 banks of 64
  memories, names kept by the app (the synth stores none), Poly-800 look

## Status

Usable with the Kiwi-3P (tested in Cubase 15 and as standalone app on Windows 11); the KiwiSix
programmer shares all of the following:

- All 87 Kiwi-3P tone parameters, editable and automatable in the DAW
- Fetch the synth's current sound, send sounds to its edit buffer; real-time edits via CC,
  SysEx for parameters without CC, FREEZE
- Lossless handling of patch data (undocumented values survive editing)
- Preset library (banks, categories, tags, favorites, search, .syx import)
- Synth memory view: read all slots, write by drag and drop with read-back
- Patch generator with 8 categories, KEEP masks, variations and history
- TEST phrase; All Sound Off before switching sounds
- TUNE: the synth plays an A with a plain waveform while the app plays the same tone at
  440 Hz (adjustable level) for tuning by ear; the synth's sound comes back afterwards
- Plugin: sound is saved with the project and sent when it opens; one instance per synth
  (others grey out, TAKE OVER by click); the track's notes are played on the synth
- MIDI loop protection
- AI control (MCP, optional): AI assistants such as Claude can read and set every parameter, play
  notes and use library and generator - see [docs/ai-control.md](docs/ai-control.md)

## Versions

Each app shows its version under the logo, with the synth firmware it was made and tested
with (e.g. "v1.0.0 · for HAWK-800 v2.68b"). The first two numbers are set by hand in the app's
`VERSION` file: the first for a complete rework, a new framework or new synth firmware, the
second for larger changes. The third counts itself: the commits that changed the app's code
(its folder, its device modules, `framework/sso_core`) since `VERSION` last changed - same
code, same number.

## Troubleshooting

Problems such as a detected MIDI loop are written to `%TEMP%\SSO-ownership\ownership.log`.
For step-by-step details (which instance owns the synth and why), create an empty file
`%TEMP%\SSO-ownership\verbose` and restart the app or host.

## Layout

| Path | Content |
|---|---|
| `framework/sso_core` | Device-independent JUCE module: parameter model, MIDI, controller, generic GUI |
| `devices/sso_kiwitechnics` | SysEx handling shared by KiwiTechnics upgrades |
| `devices/sso_kiwi3p`, `devices/sso_kiwisix`, `devices/sso_hawk800` | Device definitions and sound generators |
| `apps/SSO-Kiwi3P`, `apps/SSO-KiwiSix`, `apps/SSO-Hawk800` | Standalone + VST3 plugins (device panel only; processor/editor live in `sso_core/plugin`) |
| `tests` | Unit tests and a hardware test (`SSO_Tests --hardware "<port name part>"`) |
| `docs` | Measured MIDI behaviour of the Kiwi-3P, the KiwiSix and the HAWK-800, firmware update notes |

## Building (Windows)

Requires Visual Studio 2022 (C++ workload) and CMake ≥ 3.22.

```
git submodule update --init
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

To install the VST3 plugins, run `install-vst3.ps1` from an administrator PowerShell
(`powershell -ExecutionPolicy Bypass -File .\install-vst3.ps1`).

## License

AGPLv3 (JUCE is used under its AGPLv3 option).
