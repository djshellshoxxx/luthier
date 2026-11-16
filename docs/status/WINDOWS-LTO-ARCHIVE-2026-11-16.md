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

## Exact-source follow-up

Release run `43961668477` on `53632189ed05a56b2bba35012f883b2b522a074a`
confirmed that the single-translation-unit partition still left too little
headroom with MSVC 14.44: Free reached `0x10004B7CC` and Pro reached
`0x1004269D5`, so both again failed with `LNK1248`.

The partition now contains fourteen self-contained UI translation units. They
remain compiled with `/GL`; the final plugin wrappers still link both archives
with `/LTCG`. The test executable adds the partitioned sources back to its own
source list so test coverage is unchanged. The partition source set and LTO
flags are guarded by the release tests. All fourteen translation units pass a
local C++17 syntax compile with the release definitions. Native Windows release
qualification is still required.
