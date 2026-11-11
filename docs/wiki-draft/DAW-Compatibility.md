# DAW Compatibility

**No host has been tested manually.** The documented handling for each host (Ableton Live, Logic Pro, Cubase, Studio One, Reaper, FL Studio, Bitwig Studio) is intended behaviour, not a tested guarantee: see [HOST_COMPATIBILITY.md](https://github.com/djshellshoxxx/luthier/blob/master/docs/HOST_COMPATIBILITY.md) and [Known Issues](https://github.com/djshellshoxxx/luthier/blob/master/docs/KNOWN_ISSUES.md).

- Pro Tools needs AAX, which is not built: unsupported.
- CI runs pluginval on the VST3 (and the AU on macOS) and clap-validator on the CLAP; strictness 5 on push, up to 10 on manual dispatch ([Releasing](https://github.com/djshellshoxxx/luthier/blob/master/docs/RELEASING.md)). A workflow existing is not a result: check the run logs.
- Behaviour common to every host is covered by named unit tests listed in HOST_COMPATIBILITY.md.
