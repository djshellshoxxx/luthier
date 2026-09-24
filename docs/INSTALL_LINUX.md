# Installing Luthier on Linux

How to build, package, install, uninstall and run Luthier in place on Linux.
The packaging follows `spec/installer.md` sections 3, 4, 6 and 10; what each
section maps to on Linux, and what does not apply, is in `spec/DECISIONS.md`.

Tested on Ubuntu 24.04 (x86-64, GCC 13, CMake 3.28, JUCE 8.0.10).

---

## What you get

| Package | Contents | For |
|---|---|---|
| `luthier_<version>_amd64.deb` | VST3, standalone, factory content, desktop entry, file types, icons, licences | Debian, Ubuntu and derivatives, system-wide |
| `Luthier-<version>-linux-x64.tar.gz` | the same as a relocatable folder with `install.sh` / `uninstall.sh` | any distro, per-user or system-wide, or run in place |
| `Luthier-<version>-linux-x64-standalone.tar.gz` | the tarball without the VST3 | people who do not use a DAW (installer.md 4) |
| `SHA256SUMS.txt`, `manifest.json` | checksums and the release manifest (installer.md 10) | verification |

Both binaries need the `Resources` folder (impulse responses, parts, guitars,
tunes, fonts; about 31 MB). They look for it beside themselves, walking up to
five folders from the binary, then in `/opt/Luthier/Resources`, then in
`~/Documents/Luthier/Resources` (`Source/Support/IrLibrary.cpp`). Every layout
below satisfies that search.

### Where the .deb puts things

| Path | What |
|---|---|
| `/usr/lib/vst3/Luthier.vst3/` | the VST3 bundle; `Luthier.vst3/Resources` is a symlink to the content below |
| `/opt/Luthier/Luthier` | the standalone; `/usr/bin/luthier` is a symlink to it |
| `/opt/Luthier/Resources/` | factory content, once, shared by both |
| `/opt/Luthier/.installed_version` | written by the package's post-install script (installer.md 6) |
| `/usr/share/applications/luthier.desktop` | launcher entry (Audio / Music) |
| `/usr/share/mime/packages/luthier.xml` | the `.luthier*` and `.midprofile` file types |
| `/usr/share/icons/hicolor/{512x512,128x128}/apps/luthier.png` | icons |
| `/usr/share/doc/luthier/THIRD_PARTY_LICENCES.txt` (and `copyright`) | JUCE, VST 3 SDK, Lato and Bebas Neue licences |

### Where `install.sh --user` puts things

| Path | What |
|---|---|
| `~/.vst3/Luthier.vst3/` | the VST3 bundle; its `Resources` symlink points at the content below |
| `~/.local/share/luthier/Luthier` | the standalone; `~/.local/bin/luthier` is a symlink to it |
| `~/.local/share/luthier/Resources/` | factory content |
| `~/.local/share/luthier/.installed_version` | version marker |
| `~/.local/share/luthier/install-manifest.txt` | every path the installer wrote; `uninstall.sh` removes exactly these |
| `~/.local/share/luthier/uninstall.sh` | the uninstaller |
| `~/.local/share/{applications,mime/packages,icons/hicolor}` | desktop entry, file types, icons |

`sudo ./install.sh --system` uses the .deb's paths instead (`/usr/lib/vst3`,
`/opt/Luthier`, `/usr/bin`, `/usr/share/...`) and keeps its manifest at
`/opt/Luthier/install-manifest.txt`.

### Your own files

Presets, guitars, parts, tunes, renders and settings live in
`~/Documents/Luthier/` (`~/.config/Luthier/` holds the licence file). No
installer or uninstaller touches them unless you ask for a purge (below).

---

## Requirements

Runtime: ALSA (`libasound2`), X11 (`libx11-6`, `libxext6`, `libxrandr2`,
`libxinerama1`, `libxcursor1`), `libfreetype6`, `libfontconfig1`, and glibc /
libstdc++ from a 2024 or newer distribution. The .deb lists the exact
versions in its `Depends` (computed by `dpkg-shlibdeps` at packaging time).
JACK or PipeWire-JACK are used when present and are `Recommends`, not
required. Wayland sessions run it through XWayland.

Build (Ubuntu / Debian package names):

```sh
sudo apt install build-essential cmake ninja-build pkg-config \
    libasound2-dev libfreetype-dev libfontconfig1-dev \
    libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev \
    libgl1-mesa-dev libcurl4-openssl-dev
# for packaging and the tests:
sudo apt install dpkg-dev xvfb desktop-file-utils shared-mime-info
```

---

## Build

```sh
git clone <repo> luthier && cd luthier      # JUCE is in ThirdParty/JUCE
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build Luthier_VST3 Luthier_Standalone
```

Outputs, with `Resources` already copied beside them:

- `build/Luthier_artefacts/Release/VST3/Luthier.vst3/`
- `build/Luthier_artefacts/Release/Standalone/Luthier`

On a shared build tree, go through the lock the helper scripts use:
`flock build/.build.lock ninja -C build Luthier_VST3 Luthier_Standalone`.

To try the build without installing anything, point your DAW at
`build/Luthier_artefacts/Release/VST3` (or symlink the bundle into `~/.vst3`)
and run the standalone from its folder.

---

## Package

The one-shot way (build, package, checksum, manifest into `dist/`):

```sh
scripts/release.sh                 # everything
scripts/release.sh --skip-build    # package an existing build/
```

By hand:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLUTHIER_BUILD_PACKAGES=ON
ninja -C build Luthier_VST3 Luthier_Standalone
cpack --config build/CPackConfig.cmake -G DEB -B build/packages
cpack --config build/CPackConfig.cmake -G TGZ -B build/packages
cpack --config build/CPackConfig.cmake -G TGZ -B build/packages -D LUTHIER_STANDALONE_ONLY=ON
```

`LUTHIER_BUILD_PACKAGES` is off by default; turning it on adds install rules
and CPack configuration (`installer/linux/Packaging.cmake`) and changes
nothing about the targets themselves. Binaries are stripped in the packages;
the build tree keeps its symbols.

`THIRD_PARTY_LICENCES.txt` is generated at configure time from
`ThirdParty/JUCE/LICENSE.md`, the VST 3 SDK licence inside JUCE and the two
OFL texts in `Resources/Fonts`.

### Signing (installer.md 0.2 and 10)

Keys are not in the repository. `scripts/release.sh` signs when told where
the key is:

```sh
LUTHIER_SIGNING_KEY=<gpg key id> scripts/release.sh
```

gives each package a detached ASCII-armoured `.asc` and clear-signs
`manifest.json` to `manifest.json.asc`. A different signer (HSM, CI secret
store) plugs in through `LUTHIER_SIGN_HOOK=/path/to/script`, called once per
file and expected to write `<file>.asc`. Without either the script prints
`UNSIGNED` and the manifest says `"signed": false`.

Verify a download:

```sh
sha256sum -c SHA256SUMS.txt
gpg --verify luthier_1.0.0_amd64.deb.asc luthier_1.0.0_amd64.deb
gpg --verify manifest.json.asc
```

### Reproducibility

`release.sh` exports `SOURCE_DATE_EPOCH` from the commit time so archive
timestamps do not depend on the clock. Two builds from the same commit are
not yet byte-identical (LTO output and CPack's staging order vary); the
checksums in `manifest.json` describe the files actually published.

---

## Install

### .deb

```sh
sudo apt install ./luthier_1.0.0_amd64.deb
```

The post-install script writes `/opt/Luthier/.installed_version` and
refreshes the desktop, MIME and icon caches. Rescan plugins in your DAW.

### Tarball

```sh
tar -xzf Luthier-1.0.0-linux-x64.tar.gz
cd Luthier-1.0.0-linux-x64
./install.sh                # asks: user or system
./install.sh --user         # no root
sudo ./install.sh --system
./install.sh --help         # --vst3-dir, --no-vst3, --no-standalone, --no-desktop, --yes
```

Installing over an existing tarball install removes the old one first (its
manifest says what to remove), which is how an upgrade works.

### Run in place (portable)

Nothing needs installing: `./Luthier` runs from the extracted folder and
finds `Resources` beside it, and a DAW can be pointed at the folder's
`Luthier.vst3` directly (its `Resources` link is relative). This is the Linux
form of installer.md 9's portable install, with one difference: your
presets, guitars and settings still go to `~/Documents/Luthier` and
`~/.config/Luthier`, not into the folder.

---

## Uninstall

| Installed with | Remove with | Also remove your files |
|---|---|---|
| `.deb` | `sudo apt remove luthier` | `sudo apt purge luthier` removes nothing more (see below); delete `~/Documents/Luthier` and `~/.config/Luthier` yourself |
| `install.sh --user` | `~/.local/share/luthier/uninstall.sh` | add `--purge` |
| `install.sh --system` | `sudo /opt/Luthier/uninstall.sh` | add `--purge` (purges the invoking user's files only) |

`uninstall.sh` removes exactly the paths in the install manifest, then the
directories it created if they are empty, and refreshes the caches. The
package's `postrm` removes `.installed_version` and refreshes the caches;
it never touches home directories, so a `purge` of the .deb is the same as a
`remove` (installer.md 0.3: user data is kept by default; the explicit purge
is the `uninstall.sh --purge` above, or deleting the two folders).

---

## Tests

```sh
installer/linux/test-install.sh            # packages in dist/, else build/packages
installer/linux/test-install.sh --real     # additionally dpkg -i / dpkg -P on this machine (root)
```

No root is needed for the default run: the .deb is unpacked with
`dpkg-deb -x` into a scratch root and the tarballs are installed with
`install.sh --user` into a scratch `HOME`. It checks the control fields and
maintainer scripts, the file layout above, the symlinks, the VST3 entry
points, missing shared libraries, that the standalone starts under Xvfb from
each layout, that the installed files equal the manifest, that a re-install
and an uninstall leave nothing of the package behind while user data
survives, and that `--purge` removes it.

Not covered (installer.md 13): cross-version upgrade and downgrade (only one
version builds at a time), the file-manager double-click (needs a desktop
session, and the standalone does not yet open a file given on the command
line), and signature verification (keys are outside the repo).

---

## Troubleshooting

- **The plugin loads but has no body or cabinet sound / fonts fall back.**
  `Resources` was not found. Options -> Diagnostics reports the folder it
  used. For a hand-copied `Luthier.vst3`, make `Luthier.vst3/Resources` a
  symlink to (or a copy of) the content folder, or put the content at
  `/opt/Luthier/Resources` or `~/Documents/Luthier/Resources`.
- **`luthier: command not found` after a user install.** `~/.local/bin` is not
  on your `PATH`; the desktop entry uses the absolute path and still works.
- **No sound in the standalone.** Pick a device in Options -> Audio. Under
  PipeWire, install `pipewire-jack` or use the ALSA device it exposes.
