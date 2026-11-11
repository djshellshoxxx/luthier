# Plugin Installation Paths

Source: [Troubleshooting](https://github.com/djshellshoxxx/luthier/blob/master/docs/TROUBLESHOOTING.md) and [Releasing](https://github.com/djshellshoxxx/luthier/blob/master/docs/RELEASING.md), section "What gets installed where". The table shows Luthier Pro; read `Luthier Free` for the Free edition.

| Format | Location |
|---|---|
| Windows VST3 | `C:\Program Files\Common Files\VST3\Luthier Pro.vst3` |
| Windows CLAP | `C:\Program Files\Common Files\CLAP\Luthier Pro.clap` |
| macOS VST3 | `/Library/Audio/Plug-Ins/VST3/Luthier Pro.vst3` |
| macOS AU | `/Library/Audio/Plug-Ins/Components/Luthier Pro.component` |
| macOS CLAP | `/Library/Audio/Plug-Ins/CLAP/Luthier Pro.clap` |
| Linux VST3 | `~/.vst3/Luthier Pro.vst3` (per user) |
| Linux CLAP | `~/.clap/Luthier Pro.clap` (per user) |

A bundle copied by hand into your own `~/Library/Audio/Plug-Ins/...` works on macOS too. CLAP rows apply to builds that include CLAP; AU exists on macOS only. User data lives under `Documents/Luthier` and is never touched by an installer.
