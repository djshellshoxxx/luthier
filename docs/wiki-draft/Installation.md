# Installation

Luthier has no published release from the current source: build it (see [[Build System]]) or use a build someone gives you, and read [Known Issues](https://github.com/djshellshoxxx/luthier/blob/master/docs/KNOWN_ISSUES.md) first.

1. Copy the whole plugin bundle (a folder, for VST3) into the folder for its format. See [[Plugin Installation Paths]].
2. Rescan plugins in your host. Hosts cache their list; see [Troubleshooting](https://github.com/djshellshoxxx/luthier/blob/master/docs/TROUBLESHOOTING.md).
3. Pro and Free are separate products with separate names (`Luthier Pro`, `Luthier Free`).

Formats: VST3 and Standalone are always built; AU is built on macOS only; CLAP is built when the CLAP wrapper is present at build time. Signed and notarised installers are not produced by the build ([Releasing](https://github.com/djshellshoxxx/luthier/blob/master/docs/RELEASING.md)).
