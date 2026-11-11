# Presets

Luthier ships factory presets and saves user presets as `.luthierpreset` files under `Documents/Luthier`.

- Using the preset browser: [USER_MANUAL.md](https://github.com/djshellshoxxx/luthier/blob/master/docs/USER_MANUAL.md), section "Presets".
- File format: [PRESET_FORMAT.md](https://github.com/djshellshoxxx/luthier/blob/master/docs/PRESET_FORMAT.md).
- List the presets a build contains: `luthier-render --list-presets`, and render with one: `luthier-render --midi riff.mid --preset "<name>" --out riff.wav`. All renderer flags are in [spec/cli-tools.md](https://github.com/djshellshoxxx/luthier/blob/master/spec/cli-tools.md), section 7.

The number of factory presets is not stated here because it has not been checked against a build; use `--list-presets`.
