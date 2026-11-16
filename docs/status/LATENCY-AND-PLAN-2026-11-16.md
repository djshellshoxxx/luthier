# Latency and per-block pedal plans — 2026-11-16

This checkpoint addresses audit findings F-A3, F-B1 and F-B2. Final native
platform, IR transition, performance and release qualification remain open.

## Host notification (F-A3)

The audio callback now publishes the routing latency atomics only while it
owns the structural engine lock. It no longer invokes JUCE host/listener
callbacks through `setLatencySamples`. Preparation and the existing message
thread timer perform host notification. A failed engine admission preserves
the prior snapshot instead of inspecting a concurrent rebuild.

The regression uses a real processor listener and four oversampling factors.
It verifies zero audio-thread notifications, unchanged host latency until the
message timer runs, and eventual matching host/routing latency.

## Bypass timing (F-B1)

Board bypass keeps the configured physical delay and host latency. The settled
disabled path advances its dry delay lines, including a post-cab board whose
latency requires continued processing. The explicit report-latency preference
remains honored. Tests measure a real 32-sample impulse delay through all three
placements under both model bypass and host automation, repeatedly toggled.

## One plan per audio block (F-B2)

`beginEvents` retains one published plan for every placement in that block.
The existing completed-reader-epoch reclamation now also retains this block
plan between placement calls. A concurrent move can no longer process the
same shared pedal runtime at two placements in one block. The existing bounded
retirement mechanism remains in use.

The regression repeatedly moves a shared counting pedal between post-cab and
before-pre while a block is in progress. Across 1,024 blocks it checks that a
64-sample block never advances more than 64 samples, has no missing block, and
keeps the retired-plan count bounded.

## Evidence

All **3 tests / 2,437 checks pass** with the fixes. Compiling the prior processor
and board implementation against the same current layout/test harness produced
**76 failed checks across all 3 tests**. This included 96 samples advanced in a
64-sample block and bypass/notification failures. The old board used the extra
retention slot only as an unused null entry for compatible test compilation.

The runner uses the real processor/engine, JUCE 8.0.10 and original test framework
with a headless processor and mixed local low-memory optimization settings.
These are functional regression results, not shipping performance or native GUI
qualification. Both native platform and quick-check workflows include the three
new regression groups. The full suites and shipping validators remain required.
