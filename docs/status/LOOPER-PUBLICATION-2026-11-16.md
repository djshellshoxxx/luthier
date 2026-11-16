# Looper manifest publication — 2026-11-16

Older Windows Pro CI (`43881011314`, job `133346659249`) failed three disk-backed
loop tests at manifest creation, followed by dependent restore/read failures.
The same 28 storage tests pass locally on Linux (15,813 checks). The Windows
failure's precise interleaving was not recorded, so native Windows confirmation
remains necessary.

## Confirmed race and repair

The worker publishes the ready generation before closing the hash seqlock.
`manifest()` previously waited for that ready flag, then attempted one bounded
snapshot capture. If the worker was preempted between those two publication
stores, the flag ended the wait but capture exhausted its retries. The call
returned false before its timeout even though the write was successful.

The wait predicate now obtains the complete coherent capture. A pending snapshot
continues waiting; a complete ready, failed or replaced generation ends it.
The default one-second wait, five-second maximum, previous-hash preservation on
failure and audio-thread capture behavior are unchanged.

## Evidence

The reproducible `scripts/looper_publication_checks` harness creates a copy of
the actual production translation unit and inserts a test-controlled pause
between the ready store and seqlock close. Product sources and normal native
builds contain no scheduling hook. The harness fails configuration/generation
if the expected publication boundary changes.

With the original wait predicate, a 50 ms pause inside the one-second budget
produced three failed assertions: early return, failed result and unavailable
returned hash. The repaired predicate passes the same 24 checks. The broader
harness passes **28 tests / 13,953 checks under ThreadSanitizer**, including disk
round trips, forward/reverse windows, sparse overdubs, compaction and source
lifetime. Native allocation counting remains covered by the product test runner.

The three formerly failing fixtures now print elapsed wait, capture status and
fault flags if persistence fails. The round-trip test stops after asserting a
failed manifest/restore prerequisite instead of issuing 128 doomed read waits.
No success-path assertion or timeout was relaxed. All storage groups also run
in the early native release checks. This repairs a proven race with the observed
symptom; it does not claim the old Windows CI cause is conclusively established
or that Windows/macOS release qualification is complete.
