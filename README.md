# Smooth Synth Operator

Modern software programmers for vintage synthesizers with modern upgrades —
standalone app and VST3 plugin.

First programmer: **SSO Kiwi-3P** for the Roland JX-3P with the KiwiTechnics Kiwi-3P upgrade,
inspired by the Retroaktiv PG-2K.

## Status

Early development. Working today:

- All 87 Kiwi-3P tone parameters, editable and automatable in the DAW
- Fetch the synth's current sound, send sounds to its edit buffer
- Real-time edits via CC, SysEx for parameters without CC, FREEZE
- Lossless handling of patch data (undocumented values survive editing)

## Layout

| Path | Content |
|---|---|
| `framework/sso_core` | Device-independent JUCE module: parameter model, MIDI, controller, generic GUI |
| `devices/sso_kiwi3p` | Device definition for the Kiwi-3P |
| `apps/SSO-Kiwi3P` | Standalone + VST3 plugin |
| `tests` | Unit tests and a hardware test (`SSO_Tests --hardware "<port name part>"`) |
| `docs` | Notes, e.g. measured MIDI behaviour of the Kiwi-3P |

## Building (Windows)

Requires Visual Studio 2022 (C++ workload) and CMake ≥ 3.22.

```
git submodule update --init
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## License

AGPLv3 (JUCE is used under its AGPLv3 option).
