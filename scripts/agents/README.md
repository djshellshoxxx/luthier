# Worker scripts: parallel worktrees on one machine

These scripts let several workers (people or coding agents) build and test Luthier side by side
on one modest machine without full rebuilds and without trampling each other. They were written
and used on a 4-core, 15 GB Linux container on 2026-11-11.

| Script | Purpose |
|---|---|
| `worktree.sh <name> <branch> [base]` | Create a worktree under `$LUTHIER_WT_ROOT`, symlink the pinned JUCE and clap-juce-extensions checkouts, configure `build/` |
| `build.sh <dir> <target>...` | Build with ninja, one compile at a time machine-wide (exclusive lock) |
| `test.sh <dir> [filters]` | Run `LuthierTests` under xvfb from the build directory with a private HOME (no lock: tests may overlap builds) |
| `shards.sh <binary> <N> <outdir>` | Run the whole suite as N exact, disjoint shards in parallel |

## One-time setup

```bash
scripts/setup_linux.sh          # system packages, pinned JUCE and clap-juce-extensions, configure build/
apt-get install -y time         # optional: peak memory with /usr/bin/time -v
ccache --set-config=max_size=10G
ccache --set-config=base_dir="$(dirname "$(pwd)")"   # a common parent of the main checkout and the worktree root
ccache --set-config=hash_dir=false
ccache --set-config=sloppiness=time_macros,include_file_mtime,include_file_ctime,pch_defines
```

Configure the main checkout with link-time optimisation off for development builds
(`-DLUTHIER_LTO=OFF`), as `worktree.sh` does.

## Measured on the 4-core container

| Step | Time |
|---|---|
| First build of `LuthierTests` (844 objects, empty cache) | about 15 minutes |
| `worktree.sh` (worktree + configure) | about 24 seconds |
| First build of `LuthierTests` in a fresh worktree (98.8 % cache hits) | about 38 seconds |
| Build directory size for `LuthierTests` | about 270 MB |

## Pitfalls found the hard way

- **Run the test binary from the build directory.** With `base_dir` set, ccache passes relative
  paths to the compiler, so `__FILE__` becomes `../Source/Tests/X.cpp`. Tests that locate fixtures
  or source files through `__FILE__` (golden hashes, the looper source checks, notation fixtures)
  fail with "missing golden file" or "could not locate ..." when started from anywhere else.
  `test.sh` and `shards.sh` do this for you. CI is not affected (no `base_dir` there).
- **Give each parallel run its own HOME.** Tests touch user folders (presets, settings). Shared
  HOME between parallel runs produces failures that are not real.
- **`luthier-render` must be built for the CLI subprocess checks to run.** Without it the notation
  suite prints "LuthierRender not found beside the test runner; skipping subprocess checks".
- **Do not kill processes by matching `LuthierTests_artefacts`.** Compiler and linker command
  lines of every worktree contain that string.
- **Timing and CPU-budget tests are noisy while other workers load the machine.** Re-run a failed
  timing test on a quiet machine before believing it.
- **Line endings are mixed.** Run `scripts/fixeol.sh` before each commit.
- **Never build in a checkout while its test binary is running** (the linker rewrites the file).

## Typical worker loop

```bash
export LUTHIER_WT_ROOT=/path/to/worktrees
scripts/agents/worktree.sh my-fix agents/luthier/<workstream>/<task>
W=$LUTHIER_WT_ROOT/my-fix
scripts/agents/build.sh "$W" LuthierTests
scripts/agents/test.sh "$W" RefPedalBoardCore HostState     # suites that cover the change
```

The coordinator runs the full suite before integrating:

```bash
scripts/agents/shards.sh build/LuthierTests_artefacts/Release/LuthierTests 3 /tmp/full-run
```
