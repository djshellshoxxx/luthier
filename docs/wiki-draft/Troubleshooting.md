# Troubleshooting

The canonical page is [TROUBLESHOOTING.md](https://github.com/djshellshoxxx/luthier/blob/master/docs/TROUBLESHOOTING.md). It has sections on a plugin that does not appear in the host (folders, rescan, blocklists, architecture, macOS quarantine), uninstalling by hand, presets that do not show up, no sound, crackles and high CPU, tuning, chord voicing, timing, hanging notes, mono summing, crashes, and what to send to support.

Quick pointers:

- Plugin missing: check [[Plugin Installation Paths]], then rescan and clear the host's blocklist.
- Heavy CPU: raise the buffer size to 256 samples or more and set Options > Oversampling to 2x ([Known Issues](https://github.com/djshellshoxxx/luthier/blob/master/docs/KNOWN_ISSUES.md), "Performance").
- Report something: Help > Debug > Export troubleshooting file; the file contains settings, host and audio configuration, no audio.
- Pedalboard questions: [Pedalboard](https://github.com/djshellshoxxx/luthier/blob/master/docs/PEDALBOARD.md).
