# Phrase looper foundation: experimental engine spec

Status: DSP core and tests authored in PR #73, **not integrated into live board/UI**, **not hardware-matched**.

Goal: independent dry-through stereo phrase loop engine for future TC Ditto/BOSS RC-1/RC-5/EHX models. Model identities must not be claimed until all specific manufacturer footswitch modes, undo/redo, max duration, memory persistence, reverse/rhythm/MIDI behavior are reproduced.

Initial core: `Source/ReferencePedals/PhraseLooper.h`. Nonblocking single-command mailbox delivered at buffer boundary; states empty/recording/playing/overdubbing/stopped, separate stereo buffers, uninterrupted dry monitoring, logic-only clear, bounded preallocated buffer and automatic playback at full capacity. One-message overwrite is an explicit temporary limitation; dedicated timestamped multi-command FIFO required before footswitch double taps. Destructive overdub is a temporary limitation; use an undoable copy-on-write/overdub-delta layer system with offline compaction and no audio-thread allocation to pass genuine Ditto/RC specifications.

Validation required for production: test foot gesture input edges, double-tap/hold delays, overdub undo and redo, clickless seam by crossfade (which must preserve rhythm/phase), session persistence, storage-backed long loops; reference the real manual, timestamp sample-accurate commands, no allocations/disk on audio thread. Unit tests cover limited core only.

Source pages: https://www.tcelectronic.com/en/products/0709-AFB, https://www.boss.info/global/products/rc-1/, https://www.boss.info/global/support/by_product/rc-1/owners_manuals/, https://www.boss.info/global/products/rc-5/.


> **2026-10-21:** `PhraseLooper.h` is a prototype primitive. Production loopers use `Source/ReferencePedals/Common/LooperCore.h` + `GestureDecoder.h` + `Services/AssetStore` (IMPLEMENTATION_PLAN WP5/WP11); PhraseLooper.h is kept only until its tests migrate, then removed.
