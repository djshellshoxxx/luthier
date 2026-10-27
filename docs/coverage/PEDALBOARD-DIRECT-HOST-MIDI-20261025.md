# Direct-host MIDI fixture qualification

Source before correction: b5bec374f01ede236350300770f5721709a39831.
The production Linux products came from the saved build launched2026-10-24
15:59:32UTC and completed16:25:23UTC, exit0. The resumed2026-10-25 turn did
not rebuild those unchanged products. Their fresh CLAP and VST3 validation is
separate from the test-only recompilations described below.

## Original oracle and measured cause

The original JamPlugin.JM39_midiOutMatchesTheAudio assertion was executed first,
unchanged:42checks, one failure,63 captured band note-ons unmatched within one
sample. A diagnostic-only test-object overlay used the same production objects,
headers, resources, controls and assertions, changing only the direct-host
buffer contract and appending counters outside processBlock.

| Fixture contract | Internal generated events | Returned events | Drop count | Unmatched captured note-ons |
|---|---:|---:|---:|---:|
| Original fresh unregistered buffer per block |125|0|125|63|
| One persistent explicitly registered buffer |125|125|0|0|

The registered comparison passed the same42checks. JM43 independently reproduced
Jam=[33,29,36,31], returned tune=[],28drops; registration returned the exact same
four-note vector with zero drops, passing its original five checks. The shared
fixture correction then passed the complete17-test JamPlugin suite:1187checks.

HostMidiCapacity deliberately grants an unknown direct host only its input-used
bytes unless that exact buffer and storage have been registered while stopped.
Clear preserves the registration; replacement, swap or shrink invalidates the
caller contract. The old fixture used fresh empty buffers on later blocks, so
the final bounded host copy correctly had zero guaranteed capacity. This result
does not justify allocating or guessing spare capacity inside processBlock.

Actual pinned wrappers were inspected: VST3/AU/AUv3/CLAP reserve2048bytes;
the verified standalone patch reserves65536. Independent exact-header checks
covered1000 stable registered copies with zero operator-new allocations. This
does not establish whole-plugin realtime safety; existing legacy lock operations
and additional queue/allocation bounds remain open.

## Reused source and unchanged coverage

Fresh remote reconciliation found Claude's2026-10-25 branch
claude/peaceful-carson-vzl4e4 at6d9048b5. Its earlier b3ec7a41f1accbad83d2f25f74a131200b7b1ea5
already contained the scoped capacity fixes. Only the three direct-host test
files are reused, preserving author and donor provenance. Its blanket reference
parameter visibility exclusion and divergent production history are not merged.

MidiOutPanel and ModelGapsUi originally had five failed checks across their two
target cases. Capacity-only donor fixtures pass both full suites:15tests,
205checks; eight original conditional positive checks now execute instead of
being bypassed by absent events. Every CHECK expression and licensing branch is
unchanged. No production, parameter-ID, host-wrapper or serialization behavior
is altered by this checkpoint. These runtime results are Linux Pro evidence;
other editions/platforms still require their own validation.

## Executable evidence and commands

Local evidence is under /workspace/luthier-evidence/jm39-20261025,
/workspace/luthier-evidence/jm43-engine and
/workspace/luthier-evidence/20261025-directhost-fixtures-b5-aff71604.
Original/registered logs, exit files, source hashes and exact link commands are
retained. An immutable copy of813 b5 native link inputs plus matching executable
resources was saved before any test-object replacement.

Commands use source /workspace/luthier-toolchain/env.sh, then:

```sh
xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests JM39 --skip=Rhythm
cmake --build build --target LuthierTests -j3
xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests JM39 JM43 --skip=Rhythm
xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests JamPlugin MidiOutPanel ModelGapsUi
```

Fresh validation of unchanged b5 products passed CLAP0.3.2:21run,18passed,
0failed,3unsupported-preset-discovery skips. VST3 pluginval1.0.4 passed
strictness5, seed0x51a7, all30 combinations of44100/48000/88200/96000/192000Hz
and1/16/37/64/481/2048samples, including GUI and automation tests. Artifact
hashes stayed unchanged. This is Linux format validation, not release acceptance.

The requested private Library intake resolved the expected version but its
supported transfer helper failed before producing readable archive bytes. No
packet-recovery or private-archive publication is claimed. These conclusions
come from fresh executable diagnostics and repository source.
