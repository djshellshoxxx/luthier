# Code review findings

Reviewer branch: `claude/luthier-review`. One entry per finding; IDs are stable.
Severity: critical (crash / data loss / heap corruption), high (audible glitch, wrong
sound, feature broken), medium (wrong behaviour in some cases, RT-safety), low.

Status is one of: `fixed in <commit>`, `reported-for-<helper-branch>` (code under
active development elsewhere; not edited here), `open` (not yet fixed), or
`wontfix` with a reason.

| ID | Sev | Location | Description | Failure scenario | Status |
|----|-----|----------|-------------|------------------|--------|
| R-001 | critical | Source/PluginProcessor.cpp:943 (processBlock) | Host blocks larger than the size promised in prepareToPlay were written whole into `clickBuffer`, `tuneClickBuffer`, `backingBuffer` and `monitorBuffer`, which are sized to that promise. | FL Studio / Reaper render sends 4096 after preparing 512 with the metronome or tune count-in running: heap overflow. Fix: processBlock renders such a block in slices of the prepared size (MIDI sliced in, output MIDI merged back). Test `ReviewRegression::aBlockBiggerThanPreparedIsRenderedWhole`. | fixed in (this commit) |
| R-002 | medium | Source/PluginProcessor.cpp:1321,1379 | `apvts.getRawParameterValue (macroByIndex (m))` every block builds a juce::String from a `const char*` (heap allocation) and hashes it, 14 times per block. | Allocation on the audio thread; dropouts under memory contention. Fix: pointers cached in the constructor. | fixed in (this commit) |
| R-003 | medium | Source/PluginProcessor.h:558 | `practicePanelOpen` was a plain bool written by the UI and read by the audio thread (data race, UB). | Made `std::atomic<bool>`. | fixed in (this commit) |
