# Native verification correction

Native source6d9b5943217d6e0872f65aea6514ebf40fc789e1 compiled Windows
and universal macOS Pro. Windows focused tests and required validators passed.
Mac failed all five architecture checks before focused tests and validators
could execute. This is a command-parser failure, not a measured DSP failure.

Apple cctools consumes all tokens after `-verify_arch` as architecture names,
so the executable must come first. Correct all four scoped occurrences: five
product checks in native_diagnostics.py, both validator-binary checks in
ci_build.sh, and the existing beta packaging check in build.yml.
PR81 at f36037f14d9d6c8a6b05f597d0836b2981cfb6d6 independently supplies
the same textual correction; its generated pycache files and divergent tree
are not imported. No acceptance threshold or failed check is removed.

The real-shell command regression was RED before the shell correction; all
19 helper/release-guard fixtures now pass. Linux fixtures establish invocation
grammar and fail-closed behavior, not actual macOS acceptance. A new dedicated
`codex/luthier-macos-diagnostics` push runs only the macOS job. Windows remains
restricted to the original native diagnostic branch, preserving its successful
run without an unnecessary rebuild. Both jobs still use immutable event SHA
checkout, required validators, and explicitly unqualified diagnostic bundles.
No tag, release, deployment or production credential write is added.
