# Backend note-off admission retry — 2026-10-25

A full 192-entry PlayEventQueue note-off lane rejected a release, but MidiInterpreter cleared its held slot anyway. The actual string kept sounding and subsequent empty callbacks had no remaining release obligation. Both the immediate release path and the minimum-duration deferred path had this defect.

The fix checks addNoteOff admission before clearing held state. An immediate rejection schedules the existing deferred-release path for the next block; a deferred rejection keeps its existing obligation. Successful admission, note duration, let-ring behavior and technique notifications follow the existing path. No new members, APIs, allocations, locks, or class layouts are introduced. This proves recovery after finite saturation; continuous flooding can still defer a release while capacity remains unavailable.

## Actual processor RED/green

Base: `280e182a8f033457f3d84384b9917a024b1d1ed9`.
Evidence: `/workspace/luthier-evidence/20261025-backend-release-admission/`.

An independent source archive and all 819 verified immutable build6 inputs were used. The original processor object/constructor and every layout header remain unchanged. The control executable adds only BackendReleaseTests.o. The candidate additionally replaces only MidiInterpreter.cpp.o. This avoids the separate preview transport candidate's processor-size ABI issue.

Two new actual-processor regressions fill the backend note-off queue, then require release on a later empty block: immediate note-off and minimum-duration deferred note-off. The immediate test also verifies byte-copy capacity did not overflow; getMidiOverflowDropCount documents byte copying, not backend event admission. Existing controller/sustain assertions are retained unchanged.

- Original: 43 cases / 13,959 checks; only the two new release assertions fail (exit 1).
- Candidate: all 43 cases / 13,959 checks pass (exit 0).
- Headless ASan+UBSan: both new cases / 6 checks pass, exit 0, DISPLAY unset, no suppressions. Only the changed backend and new tests are instrumented; other dependencies are the original objects.
- Initial Xvfb ASan+UBSan: assertions pass, then LeakSanitizer reports X11/XCB allocations (exit 1); full log retained.
- TSan: assertions pass, but exit 66 with 70 reports involving convolution background-loader dependencies. Those dependencies are not fully instrumented, so this is neither a whole-host race qualification nor a clean sanitizer result. No reports were suppressed.

Logs, exact commands, binary identities, source/ABI checks and sanitizer scope are in identity.json, results.json, sanitizer-results.json, asan-ubsan-headless-result.json and corresponding logs beside the evidence source archive.

## Integration and remaining gates

Cherry-pick this isolated commit onto the coordinator's current source after review; compile the new test through the existing test-source discovery. The fix does not depend on PreviewCommandQueue, PreviewMidiState, or modified PluginProcessor headers. Fresh combined-source native and host-wrapper qualification remains necessary after integration. Whole-processor real-time qualification remains RED at the previously measured 20 trylocks per callback. No callback-lock removal, tuning ownership, full-host sanitizer, or perpetual-flood fairness claim is made here.
