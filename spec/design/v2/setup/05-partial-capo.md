# 05 Partial capo

**IDs:** SU-09, SU-10. **ED:** 5. **Status:** partial (corrected from roadmap "missing").

**Summary.** A capo that stops only the strings the player chooses, with a per-string toggle card in the setup panel.

**Current state (corrected).** The engine already holds a per-string mask:

- `TuningEngine.h` 158-170: `setCapoStringMask`, `getCapoStringMask`, `getCapoFretFor(s)` (returns `capoFret` if bit `s` is set, else 0). Bit `s` is string `s`, with 0 the high E. Default `kAllStrings`.
- `TuningEngine.h` 172: `getHighestPlayableFret(s)` already counts from the capo per string.
- `LuthierEngine.cpp` 1137 and 3831: per-string capo feeds the stability model and the stop fret.
- `StringEngine.h` 143: `setStoppedFret`, already in place.
- `Part.h` 27: the capo is an accessory, not fitted to the guitar. It needs no mask field. The roadmap's `Part.h` change is dropped.

Missing: the setup UI, persistence in the setup block, and a test pinning the per-string stop fret.

## 1. User-facing behaviour

- Capo card in the setup panel: six toggle buttons, labelled E, A, D, G, B, E in string order (string 1 high E on the left, or the order the panel already uses).
- Toggling a string on means it is stopped at `capo_fret`. Toggling off leaves it open.
- Default: all six on when `capo_fret` is nonzero (full capo, v1 behaviour).
- With all toggles off and `capo_fret` nonzero, the card shows "No strings held". The capo does nothing, and this is allowed.
- Label: "Capo 2 on strings 1 to 4" when the mask is `0b001111`.

Bit mapping: UI string 1 (high E) = bit 0 = engine index 0. Mask `0b001111` stops strings 1 to 4 (high E to D).

## 2. Engine and model

For each string `s`:

```
f_s = capo_fret   if bit s of mask is set
f_s = 0           otherwise
stop fret (frets from nut) = f_s + finger fret
```

This is `TuningEngine::getCapoFretFor(s)` (already exists) feeding `StringEngine::setStoppedFret` (already exists). The change is UI, persistence and tests.

Voicer: `RubricVoicer` reads the playable range through `TuningEngine::getHighestPlayableFret(s)` and `getCapoFretFor(s)`, never the raw mask. This answers roadmap open question 5: yes, the voicer goes through `TuningEngine`.

Intonation of stopped strings uses the saddle offsets from `06`, with the same speaking-length rule.

Limit (documented, not modelled here): a partial capo does not change the tension of the unstopped strings. The capo-bias behaviour is owned by `tuning-stability.md`.

Thread safety: `setCapoStringMask` stores a plain `uint32`. If `getCapoFretFor` is read on the audio thread (check the call site in `LuthierEngine.cpp` 3831), make the mask a `std::atomic<juce::uint32>`, or precompute the six stop frets on the message thread and read a plain array on the audio thread.

## 3. Data model and parameters

- `capo_mask`: state, 6-bit (0 to 63), default 63. Not automatable.
- Engine default `kAllStrings` (all 32 bits set) and mask 63 must give the same stop frets for the six strings. Save and load normalise to 63.

## 4. State, file format, migration

- Setup block field `capo_mask`.
- v1 presets: mask 63. A v1 full capo (`capo_fret` nonzero) reproduces v1 exactly.

## 5. Edition

Free and Pro (both).

## 6. Performance budget

- No added audio-thread cost. The per-string stop fret is a read of two plain values, or a precomputed array.
- Under 0.001 units.

## 7. Test plan

- **SU-09.** Mask `0b001111`, capo 2. Strings 1 to 4 sound the capo-2 pitch within 1 cent. Strings 5 and 6 sound their open pitch within 1 cent.
- **SU-10.** Mask 63 reproduces v1 capo output bit-identically on the factory capo presets (`memcmp`).
- Mask 0 with `capo_fret` 2: all six strings are open (no effect).
- `getHighestPlayableFret(s)` for a masked string is `capo_fret` plus the fretting range; for an unmasked string it is the full neck.
- Mask round-trip through the setup block preserves the value.

## 8. Effort and dependencies

- ED 5 (roadmap 9).
- Depends on: `TuningEngine` (exists), `StringEngine.h` 143 (exists), `06` (saddle offsets for stopped strings), the setup block (see INDEX).

## 9. Open

- Partial-capo effect on tension is not modelled (see section 2). Confirm this is acceptable for v2.0.
