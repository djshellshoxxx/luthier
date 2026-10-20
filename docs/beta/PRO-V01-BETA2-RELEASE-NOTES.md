# Luthier Pro v0.1.0 Beta 2

This beta is built exclusively from the verified merge of pull request #77.
It is published only after the full Linux x64, Windows x64 and macOS Universal
Pro builds, complete unit suites, and strictness-10 VST3/CLAP validation pass.
The workflow validates native binary architectures and complete factory content,
then publishes SHA-256 checksums and exact source provenance.

## Changes since the published alpha

- Correct the guitar/parts override ordering for explicit player selections
  made during or just after a preset load.
- Protect recorded loop audio during same-format host reinitialization.
- Preserve later cathedral early reflections through correctly sized buffers.
- Add fast focused regression smoke tests while retaining full release gates.

## Downloads

- Windows x64: standalone, VST3, CLAP and factory Resources.
- Linux x64: standalone, VST3, CLAP and factory Resources.
- macOS Universal Intel/Apple Silicon: standalone, AU, VST3, CLAP and Resources.

## Beta limitations

This is test software. Successful automated validation does not establish
compatibility with every DAW, audio interface, driver or hardware configuration.
macOS Developer ID signing/notarization and Windows publisher signing require
configured certificate credentials. The pending experimental pedalboard and
60-day trial-lock work in separate draft pull requests are not included.
Known GUI fit issues at 1920×1080 and other beta feedback remain targets for
subsequent releases; users should retain the previously working installer
until the beta has been tested with their DAW and saved sessions.

Report crashes and sound regressions with the OS, host/DAW, sample rate,
buffer size, plugin format, steps and BUILD-PROVENANCE.json source SHA.
