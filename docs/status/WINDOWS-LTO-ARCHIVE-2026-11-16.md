# Windows LTO archive repair — 2026-11-16

## Failure evidence

Exact-source release run `43927530006` reached the Windows native build on
commit `581d3e7e93a0ffa644b477f80cf1cf1e27ead65b`. Both editions failed while
MSVC 14.51 linked the shared-code static library:

- Pro: `LNK1248`, archive size `0x100251A38`
- Free: `LNK1248`, archive size `0x10003A436`

Both archives crossed the COFF librarian's 4 GiB maximum. Compilation itself
reported no failures. The release gates had not reached unit tests, pluginval,
CLAP validation, packaging or publication.

## Repair

The build matrix now pins Windows release builds to GitHub's `windows-2022`
image and its Visual Studio 2022 compiler. `windows-latest` moved to Windows
Server 2025 with Visual Studio 2026; GitHub documents `windows-2022` as the
supported way to retain Visual Studio 2022.

This change does not disable or narrow link-time optimization. Both editions,
the complete native suites, pluginval strictness 10, CLAP validation, packaging
and exact-source release guards remain unchanged. A release-guard regression
prevents either Windows matrix entry from drifting back to `windows-latest` and
also checks that release LTO and strictness 10 remain enabled.

Native Windows requalification is required. No release claim follows from the
workflow correction alone.
