# User Guide

Full screen-by-screen reference: [docs/USER_MANUAL.md](https://github.com/djshellshoxxx/luthier/blob/main/docs/USER_MANUAL.md).
This page is the short version.

## Install

Build from source (see the Developer Guide) or use a packaged build if one is available.
Copy the whole `Luthier.vst3` (or `Luthier.component`) bundle to the plugin folder:

| Platform | Location |
|---|---|
| Windows | `C:\Program Files\Common Files\VST3\` |
| macOS VST3 / AU | `~/Library/Audio/Plug-Ins/VST3/` / `~/Library/Audio/Plug-Ins/Components/` |
| Linux | `~/.vst3/` |

Pro and Free are separate products with different bundle names (`Luthier Pro`, `Luthier Free`).

## What you get

- 25 instruments (electric, acoustic, classical, bass; 7/8-string, baritone, 12-string, resonator, fretless)
- 17 tunings plus custom per-string tuning, and seven temperaments
- 23 pedals across two racks, 13 amps, cabinets, microphones and rooms
- 36 factory presets (counts as stated in `spec/README.md`)

## Playing

- Play it from a MIDI keyboard or guitar-style MIDI input. Techniques (hammer-on, pull-off,
  slide, bend, harmonic, mute, tap, bottleneck) are detected from the MIDI stream; see
  `docs/PLAYING_TECHNIQUES.md` for the controller map.
- The Easy-mode screen exposes a macro set; Advanced mode exposes everything.

## Presets and storage

User presets, renders and diagnostics are written under `Documents/Luthier` (Windows
`%USERPROFILE%\Documents\Luthier`, macOS/Linux `~/Documents/Luthier`). Preset file format:
`docs/PRESET_FORMAT.md`.

## Troubleshooting

See `docs/TROUBLESHOOTING.md` for install problems, CPU load and crashes, and `docs/KNOWN_ISSUES.md`
for open issues.
