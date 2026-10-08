# SETUP AND ERGONOMICS V2 SPEC

A guitar sounds the way it is set up and the way the player's hands fit it.
Action, relief, intonation, nut and saddle material, string gauge, string age,
pick thickness and angle, hand size and handedness all change what the player
hears and what they can play. Luthier already models several of these, and
other specs own the wear and buzz physics. This file is a delta: it lists what
exists, and specifies only the parts that are partial or missing.

The owners of adjacent physics are: `string-aging.md` (string age and restring),
`fret-buzz.md` (action, relief and buzz), `tuning-stability.md` (settling, nut
binding, bend memory), `character-wear.md` (fret wear, tuner drift, dead spots),
`part-acoustics.md` (nut and bridge materials) and `rubric-voicer` (chord
voicing and finger reach).

## 0. Ground rules

1. **Delta only.** Anything marked exists in section 1 is not re-specified. The
   tables in section 1 cite the file and symbol for each claim.
2. **Setup is state.** Action, relief, intonation offsets, handedness, finger
   finger reach and capo masks are instrument or player state. They are structural, so
   they are not host-automatable. Existing automatable parameters keep their
   IDs (`setup_action_treble`, `setup_relief`, `capo_fret`, `pick_thickness`,
   `pick_angle`).
3. **Honest magnitudes.** Each number is tagged M, I or D (`instruments-v2.md`
   0). Intonation sensitivity is derived (I) from the scale length; the rest
   are design values (D) unless stated.
4. **Physical before numeric.** Every coupling (gauge to bend, saddle to
   intonation) is a formula with a physical reason, stated in the section.
5. **No playability advice that is not computed.** Hand-size and reach warnings
   come from the voicer's constraint, not from a rule of thumb in the UI.

## 1. Status

| Item | Status | Evidence |
|---|---|---|
| Action (treble, bass) | exists | `Parameters.h` `setupActionTreble`/`Bass`; `fret-buzz.md` 7 |
| Truss rod relief | exists | `setupRelief` (`Parameters.h`); `fret-buzz.md` 7 |
| Nut material | exists | `part-acoustics.md` 115 (`nut.material`: bone 0.75, brass 0.85, graphite 0.70, plastic 0.60, Tusq 0.72); friction in `tuning-stability.md` 2.2 |
| Pick thickness, angle | exists | `Excitation.cpp` 104 (`pickThickness * 0.45 + pickAngle * 0.30`) and 148 (contact apex) |
| String age, restring one or all | exists | `string-aging.md` 234-238 (restring commands) |
| Intonation | partial | `intonation_error` (`Parameters.h` 66) is an error amount; no per-string saddle compensation |
| String-change day | partial | Restring exists; the stretch period (`tuning-stability.md` 2.1) and a combined check do not |
| String gauge to bend stability | partial | Gauge sets tension (`part-acoustics.md` 95); friction bend residual in `tuning-stability.md` 2.2 has no gauge term |
| Saddle material | partial | Saddle is a bridge part and a piezo source (`PartAcoustics.cpp` 648); no acoustic saddle coefficient found |
| Left-handed mode | missing | No hit in `Parameters.h` or `UI` for handedness |
| Hand size and reach | partial | `RubricVoicer.h` 169, 196 (`hand_span_frets` constraint); no player setting |
| Partial capo | missing | `capo_fret` is a full capo (`Parameters.h` 56-60); `GAPS.md` 577 "partial capos ... still open" |

## 2. User stories

- **U1.** I set each string's intonation at the saddle and see the cents at the
  twelfth fret.
- **U2.** I change my strings on one day: restring, stretch, retune, re-check.
- **U3.** I know whether a heavier set will hold a bend better on this guitar.
- **U4.** I play left-handed and the whole interface is my mirror image.
- **U5.** I tell the app my finger reach and see which chords I can reach.
- **U6.** I put a capo across only the top four strings and play open strings
  under it.
- **U7.** I change a saddle from bone to brass and hear the difference in
  sustain.

## 3. Engine and data model

### 3.1 Intonation compensation (partial)

Add six per-string saddle offsets, `intonation_comp_1` to `_6`, in millimetres
(-4 to +4, default 0), stored in the instrument's setup block (structural). The
offset lengthens or shortens the string's speaking length.

Sensitivity (I). At the twelfth fret the speaking length is half the scale, so
for scale `L` a change `d` in length changes pitch by:

```
cents = 1200 x log2( (L/2 + d) / (L/2) )
```

For `L = 648 mm` (25.5 in), 1 mm is about 5.3 cents; for `L = 628 mm` it is about
5.5 cents. Compensation is applied in `TuningEngine` as an offset to each
string's speaking length before the string's delay line is tuned. The
existing `intonation_error` remains the amount of random error the player's
intonation carries; the compensation is the setup, and the error is the
player's. A **Check intonation** button plays the open 12th-fret harmonic and
the fretted 12th note on each string, and reports cents for each.

### 3.2 String-change day (partial)

A workflow, not a new model. It composes three existing pieces:

1. **Restring** (`string-aging.md`): choose one or all strings; age resets to
   zero.
2. **Stretch** (`tuning-stability.md` 2.1): new strings go flat over the first
   hours. The wizard shows the expected drift (the settling curve) and offers a
   "stretch" timer, default 30 minutes, after which the app asks for a retune.
3. **Check**: intonation check (3.1), action and relief readout (`fret-buzz.md`
   7), and a tuner pass that reports which strings have moved since the last
   check.

The wizard is a single panel with three steps and a Back button. No new
parameters; it writes the existing `string_age` reset and sets the stretch
timer in UI state (not saved).

### 3.3 String gauge and bend stability (partial)

Tension at pitch `f` on a scale `L` with mass per length `mu` is
`T = (2 L f)^2 mu` (`part-acoustics.md` 95). A bend holds where the nut and
bridge friction keep the string from sliding back. `tuning-stability.md` 2.2
gives the stuck residual:

```
stuck = 0.03 x mu_friction x B        (B = bend in cents)
```

with `mu_friction` the nut's friction coefficient. The missing term is tension.
A heavier string has more tension at the same pitch, so the same friction force
holds it more firmly against the same bend-induced slip. Add a gauge factor:

```
stuck' = stuck x (T_ref / T)^0.5,     T_ref = tension of 10-46 on the same scale
```

The exponent 0.5 is a design value (D) chosen so that a 9-42 set on a 25.5 in
scale has about 1.2 times the residual of 10-46, and a 11-49 set about 0.9
times. This matches the usual player report that heavier sets hold bends
better, and it is a single term, not a model of stick-slip. The coefficient is
replaced by measurement when available.

The value is shown in the bend card as "Bend hold: good / fair / poor" from
the residual, and never as a number the player must interpret.

### 3.4 Saddle material (partial)

The saddle is a bridge part. Its acoustic effect is the sustain and treble
through the bridge termination, the same mechanism that nut material uses in
`part-acoustics.md` 115 for open strings. Add `saddle.material` with the same
table shape and values flagged D until measured: bone 0.74, brass 0.84,
graphite 0.70, Tusq 0.72, steel 0.90. The factor is applied to the bridge
termination's damping, not to pitch. Changing the saddle does not change
tuning, only sustain and attack.

### 3.5 Left-handed mode (missing)

A structural setting, `handedness` (right or left), stored in state.

- **Image.** The instrument picture and body are mirrored, the fretting and
  picking hands are swapped in the technique cards and tooltips, and the
  keyboard shortcuts (`KEYBOARD_SHORTCUTS.md`) map to the mirrored hand.
- **Strings.** The player chooses the string order from two options: the
  mirror of a right-handed string order, or the order a left-handed guitar is
  strung in. The choice changes the picture and the string labels; it does not
  change any pitch.
- **Strum direction.** Down and up strokes are labelled from the player's hand
  (`strum_direction` keeps its value; the label changes).
- **Audio.** Left-handed mode changes no sound. Two renders with handedness
  switched are bit-identical.

Mirroring is a view transform applied in the UI layer; `Source/UI/Guitar/`
draws the mirrored illustration from the same part data.

### 3.6 Hand size and reach (partial)

The voicer already limits a chord by `hand_span_frets` (`RubricVoicer.h` 196),
a fixed constant. Replace the constant with a player setting,
`player_reach_mm` (80-150 mm, default 110): the distance from index fingertip to
little fingertip in a relaxed stretch. The reach is physical, so the number of
frets it covers depends on where the hand sits, because fret spacing shrinks up
the neck (about 36 mm between frets 1 and 2, 29 mm at fret 5, 19 mm at fret 12
on a 648 mm scale; I, from the fret formula):

```
frets_covered(p) = max n such that  d(p + n - 1) - d(p - 1) <= player_reach_mm
d(n) = scale x (1 - 2^(-n/12))          (distance from the nut to fret n)
```

On a 648 mm scale at 110 mm: 4 frets from the first position, 5 at the fifth,
8 at the twelfth. The voicer uses `frets_covered(p)` in place of its constant.
A chord that needs more frets than the hand covers at its position is flagged in
the chord inspector: "Needs a wider reach than your fingers (N frets) here." The
setting is state, not automation.

### 3.7 Partial capo (missing)

Add a capo string mask: a 6-bit value stored with the capo part (`Part.h`, the
capo accessory), with 1 meaning "stopped at `capo_fret`" for that string. A full
capo is mask 63 (the v1 behaviour). A partial capo stops only the masked
strings; the others keep their open pitch.

`StringEngine` already takes "where the string is stopped, in frets from the
nut (capo included)" (`StringEngine.h` 143). The change is: for each string, the
stop fret is `capo_fret` if the mask bit is set, else 0. Intonation of stopped
strings uses the same saddle offsets (3.1). The capo mask is structural state.
Partial capos are on the capo's own card in the setup panel, with six toggle
buttons, one per string, labelled by string name.

### 3.8 Files

- Modified: `Source/Model/Workshop/Part.h` (capo mask), `TuningEngine` (saddle
  offsets), `StringEngine.h` (per-string stop), `RubricVoicer.h` (reach from
  setting), `Source/UI/CharacterPanel.h` or the setup panel (wizard, toggles,
  handedness).
- New: `Source/Model/Setup/SetupState.h/.cpp` (saddle offsets, handedness, finger
  reach, capo mask), `Source/UI/Setup/StringChangeWizard.*`,
  `IntonationCheck.*`, `HandednessView.*`.

## 4. Parameters

Existing, unchanged: `setup_action_treble`, `setup_action_bass`, `setup_relief`,
`capo_fret`, `pick_thickness`, `pick_angle`, `intonation_error`, `string_gauge`,
`string_age`.

New, appended in `// ==== BEGIN V2-SETUP params ====` only where a value must be
automatable. Everything else is setup state:

| Item | Kind | Range | Default | Automatable |
|---|---|---|---|---|
| `saddle_material` | choice (state) | bone, brass, graphite, Tusq, steel | bone | no |
| `intonation_comp_1`-`_6` | state, mm | -4 to +4 | 0 | no |
| `capo_mask` | state, 6-bit | 0-63 | 63 (full capo) | no |
| `handedness` | state | right, left | right | no |
| `player_reach_mm` | state | 80-150 | 110 | no |
| `stretch_timer_min` | UI state (not saved) | 0-120 | 30 | no |

Net new automatable parameters: none. Every new control is a state control with
a visible UI, so `everyAutomatableParameterHasAVisibleControl` is unaffected.

## 5. State, file format and migration

- The setup block (saddle material, saddle offsets, handedness, finger reach, capo
  mask) is saved with the instrument's setup, and with presets as an optional
  `setup` block (`file-formats`). A preset without it loads the defaults.
- v1 presets load with: saddle bone, offsets zero, handedness right, finger reach
  110 mm, capo mask 63 when `capo_fret` is nonzero (v1 full capo). Bit-identical
  renders (SU-M01).
- The stretch timer is UI state and is never saved.
- `string_gauge` and `string_age` are unchanged. The gauge term (3.3) is a
  formula, not a new state.

## 6. Edition

| Feature | Free | Pro |
|---|---|---|
| Intonation offsets and check | Yes | Yes |
| String-change wizard | Yes | Yes |
| Gauge-to-bend hold readout | Yes | Yes |
| Saddle material (sustain effect) | No (bone only) | Yes |
| Left-handed mode | Yes | Yes |
| Reach warnings | Yes | Yes |
| Partial capo | Yes | Yes |

Setup is a fundamental feature and stays in Free, as a rule for features a
player needs to get a guitar playing (`editions.md`). Saddle material is a
sound-shaping choice and is Pro.

## 7. Performance budget

Per `performance-budget.md` 0 units:

- Intonation offsets and saddle material are applied when the instrument is
  set up (message thread). No per-block cost.
- Gauge term: applied in the bend path, one multiply per bend event, negligible.
- Partial capo: the per-string stop is precomputed; no added audio-thread cost.
- Left-handed view: UI only.
- Total new audio-thread cost: under 0.01 units.

## 8. Tests

- **SU-01 (intonation sensitivity).** A 1 mm offset at the twelfth fret moves pitch
  by 5.3 cents on a 648 mm scale and 5.5 cents on 628 mm, each within 0.2 cents.
- **SU-02 (intonation check).** A string set 2 mm long reports +10.6 cents (sharp)
  at the 12th fret, within 0.5 cents of the formula.
- **SU-03 (string-change wizard).** A restring-all sets `string_age` to zero for
  all six strings and shows the stretch timer at the default 30 minutes.
- **SU-04 (gauge hold).** On the same scale, the bend residual for 9-42 is
  between 1.15 and 1.25 times 10-46; for 11-49 between 0.85 and 0.95 times.
- **SU-05 (hold readout).** The "good / fair / poor" band changes only at
  thresholds set in the bend card; no numeric residual is shown.
- **SU-06 (saddle material).** Brass versus bone changes the 2-second decay by
  at least 0.2 s at the same note, and changes no pitch (cents difference 0).
- **SU-07 (handedness).** Switching handedness changes no sample: two renders
  are bit-identical.
- **SU-08 (reach).** At a 110 mm reach on a 648 mm scale, the reach covers 4
  frets at the first position, 5 at the fifth and 8 at the twelfth. A chord
  needing 5 frets at the first position is flagged there and not at the twelfth.
- **SU-09 (partial capo).** With mask 0b001111 (strings 1-4), strings 5 and 6
  sound their open pitch and strings 1-4 sound a capo-2 pitch, within 1 cent.
- **SU-10 (full capo compatibility).** Mask 63 reproduces v1 capo output
  bit-identically.
- **SU-M01 (migration).** Every factory preset renders bit-identically with the
  setup block at defaults (shared with PB-M01).
- **SU-11 (visible controls).** Every new control is in the setup panel and is
  keyboard reachable (manual check, `accessibility.md`).

## 9. Effort and dependencies

ED = engineer-days, one engineer with an AI pair.

| Work | ED |
|---|---|
| Intonation offsets, check tool (3.1, SU-01, SU-02) | 6 |
| String-change wizard (3.2, SU-03) | 5 |
| Gauge hold term and readout (3.3, SU-04, SU-05) | 3 |
| Saddle material (3.4, SU-06) | 3 |
| Left-handed view and state (3.5, SU-07) | 6 |
| Finger reach and frets covered (3.6, SU-08) | 4 |
| Partial capo (3.7, SU-09, SU-10) | 5 |
| Setup block, file format, migration (5, SU-M01) | 3 |
| UI polish and keyboard path (SU-11) | 2 |
| **Total** | **37 ED, about 7.5 weeks** |

Dependencies: `string-aging.md` (restring); `tuning-stability.md` (settling,
friction term); `fret-buzz.md` (action and relief readouts); `part-acoustics.md`
(scale, mass per length, nut and bridge parts); `RubricVoicer` (reach);
`pedalboard-v2.md` (nothing; setup is independent of the board).

## 10. Open questions

1. **Exponent in 3.3.** The 0.5 exponent is a design value. Measure bend hold on
   two gauges of one guitar before shipping the readout as "good / fair / poor".
2. **Saddle values.** The saddle table is D. Source it from the research file
   for the bridge parts, or drop the saddle option in v2.0.
3. **Handedness string order.** Confirm the two string-order options with a
   left-handed player before labelling them.
4. **Reach default.** 110 mm is a design default. Ask whether the setting
   should be asked for at first run, or left at default until the chord
   inspector flags a reach problem.
5. **Partial capo in the chord voicer.** The voicer must know the mask to avoid
   suggesting a chord the capo blocks. Confirm the voicer reads the capo mask
   through `StringEngine`, not directly.
