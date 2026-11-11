# Plugin Formats

From `CMakeLists.txt`: VST3 and Standalone are always built; AU is added on macOS (`LUTHIER_FORMATS`); CLAP is built when `ThirdParty/clap-juce-extensions` is present (`scripts/setup_linux.sh` and the CI workflows fetch it, pinned to a commit in `scripts/ci_build.sh`). AAX is not built. Editions: Pro (default) and Free, chosen with `LUTHIER_EDITION` ([[Build System]]).

Validators: pluginval (VST3, AU) and clap-validator (CLAP) through `scripts/ci_build.sh validate` ([[Testing]]). CLAP uses a pinned wrapper patch to preserve same-value host write intent.
