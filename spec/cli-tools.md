# COMMAND-LINE TOOLS SPEC

Extends `spec/notation-export.md` and `spec/tab-import-export.md` onto the
command line. The offline renderer (`Tools/RenderCli.cpp`, target
`LuthierRender`, product name `luthier-render`) already renders MIDI to audio;
this spec documents the notation **conversion** and **inspection** commands
layered onto the same binary so there is one offline tool, not several.

Everything here runs offline on the main thread of a console app, reusing the
notation model and I/O that the plugin uses (`NotationImporter`,
`NotationExporter`, `PerformanceScore`, `TabFingering`). Nothing here touches
the audio thread and nothing here re-implements a parser or a writer.

## 0. Ground rules

1. **Reuse, don't reimplement.** Every conversion is `import by detected format
   → PerformanceScore → export to the requested format`, through the existing
   `NotationImporter`/`NotationExporter`. The CLI owns argument parsing, format
   detection, batching and reporting — nothing else.
2. **Never crash on input.** Any file, well-formed or not, empty or huge,
   produces a status line and an exit code, never a crash. This is the
   graceful-failure contract of section 5.
3. **Guitar-aware.** A MIDI source has no strings or frets, so MIDI→tab fingers
   the part with `TabFingering`. A source that already carries strings/frets (a
   tab, a MusicXML tab part, a Luthier-profile MIDI) keeps them. A tuning or
   capo present in the source is carried through conversion and honoured by the
   exporters.
4. **Backward compatible.** Every pre-existing flag (`--midi`, `--audition`,
   `--preset`, `--out`, `--list-presets`, `--calibrate-factory`,
   `--render-previews`, `--export-riffs`, …) keeps working unchanged. The new
   commands are additive and are dispatched before the audio-render path.

## 1. Formats and detection

| Format      | Extensions         | Import | Export | Notes                                   |
|-------------|--------------------|--------|--------|-----------------------------------------|
| MIDI        | `.mid`, `.midi`    | yes    | yes    | MIDI→tab is fingered by `TabFingering`. |
| ASCII tab   | `.tab`, `.txt`     | yes    | yes    | dialect-tolerant reader; `.txt` default |
| MusicXML    | `.musicxml`, `.xml`| yes    | yes    | tab staff by default                    |
| Guitar Pro  | `.gp`              | no     | yes    | GPIF bundle; **export only** (binary, proprietary to read — export it from Guitar Pro as MusicXML to read it back) |

**Input detection** is by file extension, case-insensitive, matching
`NotationImporter::canRead`. A `.gp`/`.gp5`/`.gpx`/`.ptb` input is reported as
an unsupported (export-only / proprietary) format, not as a corrupt file.

**Target format** (`--to <format>`) is named, not guessed. Accepted names
(case-insensitive): `midi`/`mid`, `tab`/`ascii`/`asciitab`/`txt`,
`musicxml`/`xml`, `gp`/`guitarpro`/`gpif`. An unknown target name is an error.

**Output extension** written for each target: MIDI `.mid`, ASCII tab `.tab`,
MusicXML `.musicxml`, Guitar Pro `.gp`.

## 2. Convert — single file

```
luthier-render --convert <in> --to <format> [--out <dir-or-file>] [modifiers]
```

- `<in>` is one file whose format is auto-detected by extension.
- `--to` names the target format (section 1).
- `--out` is optional:
  - omitted → the result is written **beside the input**, same basename, new
    extension (`riff.mid` → `riff.tab`). If that equals the input path
    (converting to the same format) the output basename gets a ` (converted)`
    suffix so the source is never clobbered by itself.
  - a directory (existing, or a path ending in `/`) → `<dir>/<basename>.<ext>`.
  - a file path → written there verbatim (its extension does not override
    `--to`).
- Prints one status line: `OK <in> -> <out>` or `skip <in> (<reason>)`.
- Exit code: `0` on success, `1` on failure.

## 3. Convert — batch

```
luthier-render --convert-batch <dir-or-glob> --to <format> --out <dir> [modifiers]
```

- `<dir-or-glob>` selects inputs:
  - an existing **directory** → every file in it (non-recursive) whose
    extension is a readable format.
  - a **glob** like `riffs/*.mid` or `*.tab` → its parent directory filtered by
    the trailing wildcard, then by readable extension.
- `--out <dir>` is **required** for batch and is created if absent. Each input
  is mirrored flat into it by basename: `<out>/<basename>.<ext>`. (Flat mirror,
  not a tree — a batch is a pack of files, and basename collisions across
  subfolders are not a batch's job to resolve.)
- Each input yields one status line:
  - `OK   <name> -> <out-name>`
  - `skip <name> (<reason>)` — unreadable extension, parse failure, empty, or
    (with `--no-clobber`) an existing output.
- A summary line follows: `converted N of M (K skipped)`.
- **Exit code**: `0` if at least one file converted; **nonzero (`1`) only if
  every input failed**; `2` if the input selector matched no files at all.
  A batch where some succeed and some fail is a success (`0`) — the per-file
  lines carry the detail, and CI that wants strictness uses `--validate`.

## 4. Other commands

### `--inspect <file>`
Parse the file and print what it is, using the importer's diagnostics and the
score. No output file. Prints: detected format, title/artist if present, tempo,
time signature, tuning name and open-string notes, capo, track count, bar
count, note count, number of strings, and a technique tally (which
`ScoreTechnique` types appear and how many). Any importer warnings
(e.g. "generic MIDI: strings and frets are a best guess") are printed. Exit `0`
if it parsed, `1` if it could not.

### `--validate <file-or-glob>`
Parse-check one or many files without writing anything — the CI gate. For each
file prints `valid <name> (<summary>)` or `INVALID <name> (<reason>)`.
**Exit nonzero (`1`) if *any* file is invalid**, `0` only when all are valid,
`2` if nothing matched. (Contrast with `--convert-batch`, which tolerates
partial failure; `--validate` is the strict form.)

### `--transpose <semitones>` (modifier)
Shifts every note by a signed number of semitones before export. Pitches move;
the part is **re-fingered** onto the (possibly retuned) instrument with
`TabFingering`, so a transposed tab is playable, not just renumbered. Notes
pushed outside the instrument's range are clamped and counted (reported in the
status/inspect output). Combines with `--convert`, `--convert-batch` and
`--inspect`.

### `--retune <tuning>` (modifier)
Re-frets the part onto a named tuning (keeping the pitches, changing the
strings/frets) before export. Named tunings (case-insensitive): `standard`,
`drop-d`, `drop-c`, `dadgad`, `open-g`, `open-d`, `half-step-down` (Eb),
`full-step-down` (D), `7-string`, `bass`, `drop-b`. A tuning given as an
explicit note list (`"D A D G A D"`, `"Eb Ab Db Gb Bb Eb"`) is parsed with the
tab reader's tuning parser. `--transpose` is applied first, then `--retune`.

### `--list-formats`
Prints the import/export format table of section 1 and exits `0`.

## 5. Graceful-failure contract

- Missing input file, unreadable extension, parse failure, empty score, or an
  export error are **reported, never fatal**: a `skip`/`INVALID`/failure line
  names the reason.
- In batch, one bad file never stops the others; processing continues and the
  summary + exit code (section 3) reflect the whole run.
- Malformed, truncated, binary-garbage, or very large inputs are handled by the
  existing readers (which "never throw, never crash on any input"); the CLI adds
  no path that can throw on them. The fuzz tests in section 6 enforce this.
- Output directories are created as needed. Existing outputs are overwritten by
  default; `--no-clobber` turns an existing output into a `skip(exists)`.

## 6. Testing (`Source/Tests/CliConvertTests.cpp`)

The commands are implemented as small functions over the notation model, tested
directly for speed, with at least one end-to-end subprocess test of the built
`LuthierRender` and a smoke invocation wired into `scripts/render_cli_smoke.cmake`.

Required coverage:

- **Pairwise conversion**: MIDI→tab, tab→MIDI, tab→MusicXML, MusicXML→tab,
  tab→Guitar Pro (export-only), MIDI→MusicXML.
- **Round-trip stability**: tab→MIDI→tab preserves pitches and timing within
  tolerance; MIDI→tab→MIDI preserves the note set.
- **Batch** over a directory of mixed good + bad files: good ones convert, bad
  ones `skip` with a reason, exit code per section 3; and an all-bad batch exits
  nonzero.
- **Auto-detection** of every readable extension; `.gp` input rejected cleanly.
- **`--inspect` / `--validate`** output and exit codes, including a glob.
- **`--transpose` / `--retune`**: pitches shift / re-fret correctly, clamp
  counting, named + note-list tunings.
- **Fuzz**: empty, truncated, random-byte, and oversized inputs never crash and
  always produce a status + exit code.
- **Overwrite / `--out`**: beside-input, into-dir, same-format suffix, and
  `--no-clobber` behaviour.

## 7. Flag reference, as parsed by `Tools/RenderCli.cpp`

This section lists every flag the argument parser accepts (`parseArguments`,
`Tools/RenderCli.cpp:202-260` at commit `ca14934e`) and what the code does with it.
It was written from the code. It was not checked against a run of the built tool,
because the tool was not built for this revision of the document.

### 7.1 General behaviour

- A flag that takes a value reads the next argument. If the flag is the last
  argument, the value is empty and no error is reported by the parser; the later
  checks then fail or misbehave according to the flag (not specified further).
- A path value is resolved against the current working directory.
- An argument the parser does not know prints `Unknown option: <arg>` to standard
  error and the tool exits `1`. (A positional argument is also an unknown option.)
- With no arguments, or with `--help` / `-h`, the usage text is printed and the tool
  exits `0`. Note that `--help` is only noticed after the whole command line has
  parsed, so `--help --bogus` exits `1`.

### 7.2 Flags that were not covered in sections 1 to 6

| Flag | Takes | Default | Effect |
|---|---|---|---|
| `--help`, `-h` | - | - | Print usage, exit `0`. |
| `--preset-file <file>` | path | none | Load a preset from a `.luthierpreset` file. Takes precedence over `--preset` if both are given. A missing file prints `Preset file not found: <path>` and exits `1`; an unreadable file prints `Could not read that preset file.` and exits `1`. |
| `--guitar <name>` | text | none | Select the instrument by name, case-insensitive, after the preset has loaded. An unknown name prints `No instrument called "<name>". Use --list-guitars ...` and exits `1`. |
| `--rate <hz>` | number | 48000 | Output sample rate. Clamped to 8000..192000. |
| `--depth <bits>` | integer | 24 | Output bit depth. Passed to the audio file writer; a depth the format cannot write prints `That format does not support <n>-bit.` and exits `1`. For `.flac`, a value above 24 is lowered to 24. |
| `--tail <seconds>` | number | 4 | Time rendered after the last MIDI event. Clamped to 0..30. |
| `--tempo <bpm>` | number | 120 | Tempo of a `--audition` phrase. In the code it is used only when building the audition phrase; it is not passed to the engine for tempo-synced effects, although the usage text says "for tempo-synced effects". |
| `--verbose`, `-v` | - | off | During a render, print the instrument, preset, resource summary, event count, durations, latency and a progress line at every 10 percent. No effect on other commands. |
| `--list-guitars` | - | - | Print the instrument count, then the instruments grouped by category with a description. Exit `0`. |
| `--list-phrases` | - | - | Print the built-in audition phrase names. Exit `0`. |
| `--profile <name>` | `luthier` or `generic` | `luthier` | The MIDI profile written by `--export-riffs`. Any value other than `generic` (case-insensitive) selects the Luthier profile. No effect on other commands. |
| `--write-rhythm-resources <dir>` | path | none | Write every built-in rhythm pattern as `<dir>/Rhythm/<name>.luthierpattern` and every genre kit as `<dir>/Genres/<name>.json`. Prints the two counts. Exit `0`; `1` if a directory cannot be created or a file cannot be written. |
| `--normalise [dBFS]`, `--normalize [dBFS]` | optional number | off; target -1 | Scale the render so its peak reaches the target. The number is read only if the next argument does not start with `-`. A negative target therefore cannot be given: in `--normalise -3` the `-3` is not consumed and is then reported as an unknown option (exit `1`). In practice the choices are the default of -1 dBFS (flag alone) or a non-negative number, which is clamped to the maximum of 0 dBFS. The target range is -24..0 dBFS. A silent render (peak below 1e-6) is not scaled. |
| `-o` | path | none | Short form of `--out`. |

### 7.3 Behaviour of the render path

The render path runs when no other command claimed the command line. Commands are
dispatched in this order, and the first match wins: `--render-previews`,
`--list-formats`, `--convert`, `--convert-batch`, `--inspect`, `--validate`; then, after
the audio engine is constructed, `--list-presets`, `--list-guitars`, `--list-phrases`,
`--calibrate-factory`, `--write-rhythm-resources`, `--export-riffs`; then the render.

For a render:

1. `--out` is required (`No output file. Use --out <file>.`, exit `1`).
2. One of `--midi` and `--audition` is required (`Nothing to render. ...`, exit `1`). If
   both are given, `--audition` is used.
3. The preset is applied, then the instrument, then the sequence is built. A failure in
   any of these prints a message and exits `1`. A MIDI file that cannot be read, or has
   no events, also exits `1`.
4. The engine runs in blocks of 512 samples. Rendered length is the end of the sequence
   (at least 0.25 s) plus the tail. Output is always stereo.
5. The output format follows the extension: `.aiff` or `.aif` writes AIFF, `.flac`
   writes FLAC, anything else writes WAV. The output file is deleted first if it exists
   (`--no-clobber` applies to conversion only, not to rendering). The parent directory
   is created.
6. On success it prints the path, the length, rate, bit depth and the peak in dBFS, and
   exits `0`. If the file cannot be created or written, it exits `1`.

### 7.4 Exit codes of the utility commands

| Command | `0` | `1` | `2` |
|---|---|---|---|
| `--render-previews <dir>` | previews written | could not write them | - |
| `--calibrate-factory <file>` | table written | table could not be written | calibration incomplete (any preset failed or the counts do not match); the destination file is left unchanged |
| `--export-riffs <dir>` | every factory riff written | factory riffs not found, or fewer riffs written than exist | - |
| `--write-rhythm-resources <dir>` | written | a directory or file could not be written | - |
| `--list-presets`, `--list-guitars`, `--list-phrases` | always | - | - |

Conversion, inspection and validation exit codes are in sections 2 to 4.

`--render-previews <dir>` writes `<dir>/Previews` (audio plus `previews.json`) and
`<dir>/descriptor-calibration.json` according to the usage text; the second file is
confirmed in `Source/Presets/Preview/FactoryPreviews.cpp`.
