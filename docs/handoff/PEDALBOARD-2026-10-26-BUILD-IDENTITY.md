# Source-scoped build identity — 2026-10-26

Implementation commit `2283f627e43d4b0732b6175c44cebe9fb4c21068`, based on the
completed DD-3T checkpoint `26696a8527e3b83bf1483722e4dd7f862f781421`.
Repository `djshellshoxxx/luthier`, same existing `/workspace/luthier` executor.
One coordinator edits CMake/diagnostics probes; one independent reviewer worked
read-only and found no required implementation issue. No Claude API is used.

## Change and preserved identity

`CMakeLists.txt` now appends `LUTHIER_BUILD_STRING` only to the absolute
`Source/Support/Diagnostics.cpp` source. Both target-wide definitions were
removed. The original cache/explicit override, environment, Git and `dev`
resolution block is byte-identical. No fixed replacement label is introduced.

That source is compiled into plugin shared code, tests and renderer. JUCE/CLAP
wrappers link shared code, so they need no inherited macro. The runtime getter
`getFullVersionString()` is unchanged. Troubleshooting reports and their UI
tooltip use it; About displays the marketing version separately and is unchanged.
`CrashReporting.cpp` has only a commented macro occurrence.

All DD-3T, Phase90, node, convolution-fixture and other runtime source hashes
remain unchanged. No DSP, bypass, parameter/state, CPU/numeric/memory acceptance
limit, model registration or plugin-format behavior was modified. Previous
DD-3T results remain source-specific; these checks do not replace product QA.

## Measured dependency RED/GREEN

Two controlled, explicitly named diagnostic cache overrides were compared:
`metadata-check-A` and `metadata-check-B`. They are test identities, not source
commit claims or release artifacts. Actual generated compiler commands were
keyed by output/target; all 1,670 commands were compared.

| Target | Before: stamp-sensitive commands | After |
| --- | ---: | ---: |
| Plugin shared code | 508 | 1 Diagnostics |
| Standalone wrapper | 8 | 0 |
| VST3 wrapper | 8 | 0 |
| VST3 helper | 1 | 0 |
| CLAP wrapper | 1 | 0 |
| Test runner | 826 | 1 Diagnostics |
| Renderer | 315 | 1 Diagnostics |
| **Total** | **1,667** | **3** |

The original graph check is a preserved RED; the scoped check passes. Old
configure times were 2.76/2.70 seconds; scoped configure times 2.80/2.90 seconds.
No before/after full compilation speedup is claimed from configuration timing.

Actual root Ninja object builds, at `-j3`, executed all three Diagnostics targets:

| Check | Compiles | Wall | Child CPU | Largest child RSS | Peak charged memory |
| --- | ---: | ---: | ---: | ---: | ---: |
| Fixture A metadata objects | 3 | 7.82 s | 22.74 s | 688 MiB | 6.87 GiB |
| Stamp-only A→B metadata objects | 3 | 5.22 s | 15.08 s | 688 MiB | 6.87 GiB |

The existing cgroup quota is four cores and its memory limit is 16 GiB. Charged
memory includes caches; maximum child RSS is not aggregate compiler RSS. No OOM
event was recorded. This measures three independent metadata compilations, not
the worst-case memory of every JUCE/DSP unit or safe full-build `-j4` behavior.

**One-time migration:** removing the old target-wide definition changes 1,667
existing command hashes once, even at the same B identity. Full product targets
therefore still need their normal rebuild/qualification after migration. No
Ninja logs/hashes were falsified to mark those commands clean. The measured
three-command behavior is for subsequent stamp changes under the new rules.
No old full Release rebuild was executed just to obtain a timing comparison.

## Exact runtime identity checks

`docs/coverage/probes/build-stamp-identity.{cpp,py}` compile the actual unchanged
Diagnostics source with its root-generated flags and test-only function/data
sections. The tiny main is compiled using a non-Diagnostics command and asserts
that no stamp macro leaks into it. Matching existing JUCE objects/configured
libraries supply real dependencies. No substitute getter, library stub, new
software or dependency is used.

Both exact argv expectations pass:

- configured A → `0.1.0+metadata-check-A`, exit 0;
- configured B → `0.1.0+metadata-check-B`, exit 0.

Passing stale A to the B probe returns 1 while printing the actual B identity.
This would catch a wrong suffix that the existing prefix/plus/nonempty unit test
would miss. The initial core-only diagnostic link failed on actual JUCE Colour
initializers; that harness failure and its correction to real cached module
dependencies are preserved separately. Root metadata object compilation passed.

After fixture checks, the cache and environment override were cleared through
ordinary CMake configuration for Pro, Free and ASan build directories. Their
old explicit labels are recorded in the evidence, not reused as new identities.
The final post-documentation-commit check must configure without either override
and compare the actual getter against `0.1.0+<git rev-parse --short=8 HEAD>`.
Its exact terminal source SHA and readback are stored in `FINAL-GIT-IDENTITY.json`
in the private evidence checkpoint. That identity readback is the final authority;
neither a test fixture label nor a frozen old commit is the reported build.

These are focused existing-build Linux diagnostics and compile-graph checks,
not a new complete product, full-suite, Windows/macOS, host, manual UI/device,
sanitizer or release qualification. All existing release gates remain open as
previously recorded; no publication, CI, source push or merge occurred.

## Reproduction and preservation

Evidence: `/workspace/luthier-evidence/build-stamp-20261026/`. It includes the
original CMake source, before/after command maps, object build commands/timings,
memory samples, real runtime probe results, negative control, restoration logs
and hashes. The same-identity private checkpoint retains the completed DD-3T
bundle and its earlier evidence. No build caches, binaries, credentials, full
private research packs or personal troubleshooting dumps belong in the ZIP.

Example explicit fixture configuration (diagnostic only):

```sh
source /workspace/luthier-toolchain/env.sh
cmake -S . -B build -DLUTHIER_BUILD_STRING:STRING=metadata-check-A
ninja -C build -j3 \
  CMakeFiles/Luthier.dir/Source/Support/Diagnostics.cpp.o \
  CMakeFiles/LuthierTests.dir/Source/Support/Diagnostics.cpp.o \
  CMakeFiles/LuthierRender.dir/Source/Support/Diagnostics.cpp.o
python docs/coverage/probes/build-stamp-identity.py build <probe-dir> \
  0.1.0+metadata-check-A
# Restore actual Git-derived identity before production identity validation:
env -u LUTHIER_BUILD_STRING cmake -S . -B build -U LUTHIER_BUILD_STRING
```

The temporary executor outage and `/tmp`-only permission notice interrupted the
work after baseline capture, before source edits. Work resumed only after an
explicit runtime `/workspace` permission restoration and same-checkout/process
reconciliation. No denied write, process executable read, extra UI stress,
Actions access or Library input transfer was retried. No VM, paid infrastructure,
credential/security setting, license or attribution change was introduced.
