# DECISIONS

Judgement calls made during the autonomous run, one line of reason each.
Older decisions live in `GAPS.md` ("Decisions made where nothing had
chosen") and `ambiguity-resolutions.md`.

- **2026-09-22 — Test harness owns its own `RangeState`.** `PresetManager` now
  takes the range state by reference; the harness mirrors the processor rather
  than faking one, so the preset tests exercise the real load order.
- **Right-click "Restrict to stock range" follows `advanced-ranges.md` 4, not
  gui-integration 16.** Section 16 says "only if unlocked at preset level";
  advanced-ranges 4 says restrict is offered only for a control in the
  per-control list. The more specific spec wins; a family-level lock lives on
  Options RANGES.
- **The locked-range notice is a `BubbleMessageComponent` at the control.**
  advanced-ranges 6.3 asks for an inline notice at the control, not a banner;
  a bubble anchored to the knob is that without every panel hosting a notice.
- **The Options RANGES master toggle clears per-control unlocks.** Locking is
  meant to take them away, and unlocking all makes them redundant.
- **Controls re-attach on a range-generation counter polled by the editor's
  4 Hz timer.** A SliderAttachment copies the range once; the swap happens
  inside preset loads that know nothing about windows.
- **Fixed the processor undo stack's off-by-one** rather than working around
  it: one action could not be undone and the first undo after two reverted
  both. Each entry now holds its before-state and, once undone, its after-state.
- **No separate "advanced-range clamped" banner (gui-integration 15).** A preset
  load cannot clamp here: values are stored normalised against the file's own
  `ranges` block, which is applied first. The only clamps are explicit narrowings
  (RANGES lock, right-click restrict, per-row Clamp), and each reports at the
  place it happened - a confirmation with the count, or a bubble at the control.
- **Tab-header padlocks wait for their tabs to hold physical parameters.**
  gui-integration 21 names CHARACTER and WORKSHOP; today only `amp` and
  `circuit` have ranges and neither lives in a tab. `RangesUi::drawPadlock` is
  the shared glyph for when they do (TODO items 3 and 7).
- **GuitarCircuit is a nodal (MNA) solver with trapezoidal companions, not a
  biquad pair.** volume-knob-interaction 1.1 asks for H(s) bilinear-transformed;
  trapezoidal companions *are* that transform, applied to the whole network
  without factoring a 5th-order H(s) (coil, Cp, tone cap, bleed cap, cable).
  Exact for a fixed knob, fixed-size, allocation-free.
- **Recomputed on the audio thread between blocks, not the message thread.**
  The parameter bridge runs there; a 5x5 inverse on change is allocation-free
  and cheap. ground rule 0.5's real requirement - no audio-thread allocation -
  holds.
- **Pot sections under 1 k are wires.** A near-zero section between two
  capacitive nodes maps to a pole beside z = -1 (Nyquist ringing that barely
  decays). Shorting them changes nothing audible against 500 k-scale parts.
- **The coil's resonance moved from PickupEngine into GuitarCircuit.** It
  depends on the load, which is the point of the spec; the pickup engine now
  produces EMF. Selected pickups combine as parallel impedances.
- **50s wiring puts the tone control on the wiper.** 3.1's text says "input
  side rather than wiper side", which is the modern wiring; the audible result
  it describes (keeps its top as volume comes down) is what real 50s wiring -
  tone on the wiper - does, so the physics was followed.
- **Active tone corner** runs from the tone cap's resonance with the coil
  (fully off) to above the audio band (fully open). 1.4 says "same nominal
  corner" without defining it; this ties it to the parts the user chose.
- **"Bypass is neutral" is measured against the bare coil.** Active with pots
  at 1.0 cannot be flat outright while 1.4 keeps the coil's resonance "put";
  the test asserts the buffered circuit adds nothing to the coil, and that a
  coil-less (piezo) buffered circuit is flat outright.
- **Level test's "taper prediction" is the loaded resistive divider** (wiper
  against the amp input) - the pot's own behaviour at a frequency where the
  coil and capacitors are out of the picture.
- **Capacitor parameters are in nF**, not F: a float of 2.2e-8 prints "0.00" in
  host automation lanes.
- **The spec's "+9 parameters" is +11.** Its own table lists eleven new IDs;
  the count test is 363.
- **Pot/cap dropdowns are StandardValueChoice combos beside a knob.** The combo
  offers the values people buy and reads "Custom" otherwise; the knob is how a
  custom value is set.
- **Circuit tests measure presence (4 kHz vs 100 Hz), not a -3 dB corner.**
  On the spec's own reference parts, rolling 10 -> 7 takes the 4 kHz peak from
  +7.0 to +1.2 dB and loses 3.8 dB at 5 kHz (ground rule 4's "a few dB"), but
  flattening the resonance moves the -3 dB crossing *up* (6.9 -> 8.8 kHz). The
  Kinman bleed holds presence within 0.1 dB. Tone at 0 honks near 600 Hz, so
  its corner is not monotonic while its top is. The tests assert what the
  spec's words mean ("darkens", "keeps top", "sweeps down").
- **"No tone setting above unity" is read against the open guitar's peak.** The
  loaded coil already peaks above the EMF; the check is that no tone setting
  makes the loudest point louder.
- **Advanced amp ranges: the tone stack stays on 0-1 and the part beyond is
  extra shelving/peaking (18 dB per unit)**, because a pot position outside 0-1
  is a negative resistance. Gain and master keep climbing past the knob at
  24 dB and 12 dB per unit.
- **`pick_material` keeps its shipped 12-choice list.** Adding Ultex, Tortex
  and stone (pick-noise.md 2.1) would change every saved preset's normalised
  choice value. Felt gets the softest numbers; thumbpick plays as celluloid;
  finger, thumb, brush and slide materials play as fingers.
- **`pick_thickness` / `pick_angle` stay declared 0-1** (advanced-ranges 1.0)
  and map to mm (log, 0.38-3.0) and degrees (0-60); their advanced ranges are
  where the mapping reaches 0.1-10 mm and 89 degrees.
- **`pick_material` and `use_fingers` were dead parameters** (shown, never
  read). They are now applied every block; a finger material plays as fingers
  whatever the switch says.
- **Chirp fires as the pick leaves the string at the pluck**, not at note-off:
  "release" in pick-noise.md 4 is the pick's release of the string.
- **Finger squeak replaces the string's own glide noise.** `noise_slide` now
  only drives bottleneck friction (Technique::SlideGuitar); otherwise every
  legato slide would squeak twice.
- **Noise levels are calibrated at the output**, not assumed: a note-reference
  constant and a click injection gain are held by tests that measure the click
  (~30 dB under the note) and a squeak (20-30 dB under) through the engine.
- **Engine reset restarts the noise event sequence**, so the same performance
  from the same start is sample-identical (the preset round-trip test).
- **The CHARACTER tab's winding selector edits `string_material` directly** until
  the Workshop exists; string-squeak.md 9 says it mirrors the Workshop's string
  part and should edit that part once guitar-workshop lands.
- **Multi-parameter actions are one undo step** via
  `LuthierAudioProcessor::ScopedUndoAction`, which suppresses per-gesture undo
  entries while it lives. Found when undoing a squeak style reverted one value.
- **Fret buzz is +13 parameters, not +16**: fret-buzz.md 7 lists 13 IDs (six
  nut depths among them). A 12-string's pairs share nut depths (string % 6).
- **`fret_action` is superseded by the setup geometry** and stays declared
  only for host automation compatibility; the string's in-loop contact clipper
  now takes its threshold from the setup's mean action, and `fret_buzz` stays
  as that clipper's amount.
- **Buzz calibration: 2.4 mm of displacement per unit of string level**, chosen
  so fret-buzz.md 6.1's styles behave as described (Needs a tech buzzes at
  velocity 100, Player-friendly only at 127, Clean / high never).
- **Relief test uses a high nut (1.0 mm).** With a normal nut, open strings buzz
  on the first frets whatever the relief - true of real guitars too; relief
  decides where a guitar rattles once the nut protects the low frets.
- **`slide_guitar` is re-pointed as `slide_enabled`** (slide-guitar.md 7): two
  switches for one mode would disagree. Slide is +7 parameters, not +8.
- **Hybrid mode: one string is under the bar at a time.** A slide note on
  another string while the bar's string sounds is played fretted (legato if it
  came from somewhere, else a pluck), so it can squeak.
- **Slide vibrato reads `vibrato_depth` as tenths of a millimetre** of bar
  travel (its declared range is 0-80 "cents"; 3.2 says mm).
- **The bar moves linearly and arrives when the move ends**, driven block by
  block from SlideEngine, instead of riding the string's exponential glide.
- **Per-note sustain scale**: per-block modulation used to reset every string's
  sustain scale to 1.0 each block, silently undoing character-wear dead spots
  and fret wear. It now multiplies a per-note value set at note-on (which also
  carries the slide's damping).
- **2026-09-23 spec update**: nine phase-5b technique specs landed and are
  queued in TODO. The nine phase-2b specs INDEX and the brief now list
  (string-aging ... tuning-stability) are **not on disk**; they are recorded as
  blocked rather than written from their one-line INDEX descriptions.
- **CharacterPanel sizes itself to its content.** Nothing ever set its height,
  so the workspace viewport held it at its 80-point minimum. It now fits on
  construction and whenever the SLIDE group appears or disappears.
- **The SLIDE group carries mode, damping and assist as well as spec 7's
  list** - those parameters need a home and the group is it; assist is
  labelled "(aid)" per slide-guitar.md 4. Changing the mode resets damping
  behind to that mode's default (lap steel/dobro 1.0, others 0.55).
- **The low-action warning offers a "Use Slide setup" button** that applies
  the style only when clicked; the setup is never changed automatically (6).
- **Factory guitars: factory-content.md's 15 plus one per remaining
  GuitarType (27 files).** guitar-workshop.md 0.6 says every enum entry ships
  as a guitar file; factory-content.md 2 lists 15. Both hold this way.
- **Part names avoid `:` and `/`** ("Kluson 15 to 1", "80-20 Bronze"): they
  are file names and those characters are illegal on Windows.
- **`WorkshopGuitar` is guitar-workshop.md 3's `GuitarSpec`.** The build's
  `GuitarSpec` is the compiled engine description; mapSpec re-points it
  (3.1's "widening, not replacement") rather than renaming it.
- **Part fields not yet consumed**: tuners (ratio, mass, stability, locking),
  nut friction and width, fretboard radius and thickness, pickup coil_turns and
  pole shape, bridge spring_count, pickguard plies. Their consumers are
  tuning-stability.md (not on disk), board-radius clearance, and bench UI;
  part-acoustics 11's "every field moves something" test covers the mapped
  fields and these are tracked here rather than faked.
- **Termination brightness is normalised to nickel-silver frets and a bone
  nut**, so a guitar of reference parts sounds as its compiled type did.
- **Magnet pull**: sustain x 1/(1 + 0.09 P), pitch -0.9 P cents, P the sum of
  pull x (damping/0.032) x (2.5 mm / height)^2 over the pickups - puts a close
  ceramic at ~25% shorter sustain and a few cents flat.
- **2026-09-23 (user): Luthier gets its own guitar-shop theme, overriding
  `theme.md` for this plugin only.** `theme.md` is the shared house style of
  the plugin family and is left unchanged for the others;
  `spec/proposals/visual-polish.md` section 6 wins where it differs, and
  `theme.md` still supplies layout, spacing and the value arc's role. The
  user also approved model-specific knob caps and a user/follow-the-guitar
  accent colour.
- **The default guitars ship Player-friendly.** Type 0 (Vintage Double-Cut) and
  onboarding's Les Paul (Vintage Single-Cut) carry 1.6 / 2.0 / 0.20, so a
  fresh instance and a fresh install read fret-buzz.md 6.1's ship default.
- **`cpuStaysWithinBudget` takes the best of three renders.** Wall-clock time
  on this shared laptop picked up a virus scan once (92% of real time in the
  full run, 28% alone); the fastest run measures the engine.
- **A state load keeps its own parameters; picking a guitar type writes the
  guitar's.** The parameters overlapping parts are refinements saved with the
  preset. 6d reloaded the guitar file on every `applyAllNow` and wrote over
  them (randomise, range changes, every preset load). The loader now keys on
  reference + override and does nothing when the same guitar is re-applied.
- **Preset `guitar.reference` is "Factory/<Family>/<Name>" or "User/<Name>"**,
  resolved user first, like parts. A missing file falls back to the type's
  factory guitar and raises the missing-part notice.
- **Old presets' migrated pickup placements become `guitar.override`**: they
  are an edit of the file's guitar and must survive a re-save.
- **Reset loads the default type's factory guitar under the defaults.**
- **Save As Guitar's "Bundle parts" is a second button** ("Save with parts")
  writing the parts to `<name> parts/<Category>/` beside the guitar file.
- **Save As Part fits the saved part**, so the inspector stops showing the
  slot as an unsaved edit.
- **Workshop edits write the guitar's values over the overlapping
  parameters** (as a type change does). Known limit until the bench's setup
  section exists: a setup tweak made on the SETUP controls is replaced by
  the guitar's setup when a part is swapped.
- **The 5 ms click-free swap parks the audio thread.** A true crossfade
  would need two engines. Instead a guitar change fades the output out over
  5 ms (raised cosine), the audio thread renders silence without entering
  the engine while the message thread rebuilds it, then fades in over 5 ms.
  This also stops the rebuild racing the audio thread, which it did before.
  No wait when no audio is running or the caller is the audio thread; a
  250 ms cap if the host stalls. Notes arriving while parked (a few ms) are
  dropped.
- **Per-string arrays in part files use the engine's string order: index 0 is
  the high E** (engine.md 1: "channel 1 = high E (string index 0)"). The
  factory `gauges_in` lists were already in that order; the partial capo's
  `string_mask` was written low-string-first and clamped B, G, D. It now reads
  `[F, F, T, T, T, F]`, which is A, D and G, the usual 3-string partial capo.
- **Assistant agents (user request, 2026-09-23).** Two helper agents work in
  parallel on tasks that need no compiler (this machine can only run one
  build): `docs/spec-coverage.md`, and the body / headstock outline data in
  `Tools/body_outlines.py`. Only the lead builds, tests and commits.
- **Conflicts from the coverage audit (`docs/spec-coverage.md`, 2026-09-23):**
  - C-02 (nine phase-2b specs referenced but absent): stays **blocked**. Writing
    nine physics specs from one-line INDEX summaries would be inventing
    features, which the brief forbids without review. The dependent references
    in the phase-5b specs are built without those modules and noted.
  - C-09 (guitar swap parks the audio thread instead of ui-wiring 6's atomic
    swap with a coefficient crossfade): keep the park for whole-guitar loads
    (a string-count change cannot crossfade in one engine), but **queue**
    notes that arrive while parked instead of dropping them. Part swaps that
    keep the string count move to ui-wiring 6's off-thread build and
    block-boundary swap when the Workshop bench (step 7) lands.
  - C-16: ship `tune-builder.md` 10's **ten** tune templates; onboarding's
    "12" is a miscount (it names no extra two).
  - C-19: genre kits become `.luthierkit` files (magic `luthier.kit`) with
    factory-content 6's fields, added to file-formats when kits are next
    touched.
  - C-32: the column-4 strip gets **14 tabs**, TECHNIQUES before HELP. The
    techniques delta was written after the 13-tab rule and the brief says to
    apply it on top of gui-integration.
- **Guitar illustration materials.** `proposals/visual-polish.md` 1 (approved)
  adds lighting to `guitar-illustration.md` 0.2's flat language: key light,
  sheen, metal gradients, short shadows. Where the two differ the approved
  proposal wins; High contrast renders flat.
- **A burst follows the outline.** Section 11.2 draws bursts as one radial
  gradient. A sprayed burst darkens along the body edge, which a circle
  cannot do on a single-cut, so the renderer adds the radial centre plus
  stacked edge strokes clipped to the body. It reads as a real burst.
- **Strings draw over the neck.** Section 5 lists strings (14) under the neck
  and fretboard (15-17), which would hide them along the neck. The renderer
  draws neck, fretboard, frets, nut and headstock first, then the strings,
  then the tuner posts over the string ends.
- **No neck-plate bolts on the top view.** Section 6 asks for four bolt dots on
  a bolt-on neck; they are on the back of a real guitar. The top view shows
  the pocket seam instead, a set neck a rounded heel and a through-neck its
  laminate lines under a see-through finish.
- **Fanned frets** are drawn when the guitar is tagged `fanned`, the neck is a
  multi-scale part, or it has `scale_length_treble_mm`; without that field
  the treble scale is the bass scale less 38 mm, with fret 7 perpendicular.
- **Family templates are the factory guitars 12.2 names** (Vintage Double-Cut,
  Grand Auditorium, Classical, P-Style Bass, Resonator Steel). There is no
  `extended` family in the parts model (`getGuitarFamilies`), so the
  7-String Modern is an electric, not a family template. A family switch
  takes the template's body style, setup, finish and hardware; keeps the
  character seed and every part that suits the new family.
- **Picking a guitar changes the tuning only when the tuning cannot hold its
  strings.** A bass gets its bass tuning (it used to keep Standard and play
  six guitar-tuned strings); Drop D survives a change between six-strings. A
  12-string's preset tunes its six courses instead of cutting it to six.
- **A third assistant (user request, 2026-09-23)** writes whole model features
  in new files without compiling (Tune Builder first); the coverage assistant
  moves on to midi-export.md's model the same way; the outline assistant owns
  the headstock data. The lead compiles, integrates, tests and commits.
- **Pickup heights clamp at 0.8 - 6 mm.** workshop-ui.md 4 says 0.5 - 6.0;
  guitar-illustration.md 19 says not below 0.8 without advanced ranges, and
  the illustration spec ranks higher on the visible guitar. 0.8 it is until
  pickup height joins an advanced-range family.
- **A bench drag moves the pickup live and commits once.** Rebuilding the
  engine per mouse move would park the audio 30 times a second; the move
  goes to the pickup engine lock-free each step and the guitar is committed
  (one undo entry, one swap) on release.
- **Body and cab impulse responses are reloaded only when the file changes,
  and a load settles JUCE's 50 ms crossfade before returning.** A guitar
  rebuild took 66 ms (all of it silent, parked) and now takes ~14 ms; the
  cabinet's reset() now clears its convolution tails too (it did not).
- **The spectrum delta measures after the amp and cab** (what the player
  hears), on a fixed pluck of 4096 samples with the committed render cached,
  and ignores bands 60 dB under the loudest. A string-material swap reads as
  small because part-acoustics.md 8 maps material to brightness and squeak
  only, not to the pickup's pull on the string; that is the spec's call.
- **Easy mode keeps its five older macro knobs** (Attack, Body, Drive, Tone,
  Space) in the Playing strip beside Humanize and Character. gui-integration
  3 does not place them and 3.6 does not omit them; spec.md's Easy mode has
  them, and the brief forbids dropping a feature silently.
- **Easy mode's separate fretboard is gone**; 3.1 has the illustration's own
  fretboard show played notes. The fretboard stays in Advanced mode.
- **Wet/dry mixes the DI against the rig before the master limiter**, not
  after it (3.4 says "post-master"): after the limiter the dry signal could
  push the output over the ceiling. Width is mid/side at the same point.
- **The character macro is `macro_character`, a parameter**: the CHARACTER
  tab's amount slider writes it too, so the amount is automatable and there
  is one writer.
- **New parameters are appended at the end of the layout** (393 now): hosts
  may address automation by index.
- **Trademarks out of every shipped name (factory-content.md 0.1).** Display
  names change, choice indices do not, so presets and automation are
  unaffected. Internal enum identifiers (`GuitarType::Stratocaster`,
  `TrebleBleed::kinman`) are not shown and stay. "American Twin" is kept: it
  is qa-polish.md 11's own example of an acceptable name. Renamed factory
  parts and guitars keep loading under their old names through
  `PartLibrary::renamedFactoryPart` / `renamedFactoryGuitar`, the only place
  the old names appear (marked so the scan skips them). A final legal review
  is still a ship-gate item (qa-polish.md 11).
- **A reset makes the next render repeat exactly, even across a preset
  change** (midi-export.md 12's round-trip null, <= -60 dBFS for every
  factory preset, found 4 presets failing against themselves at -25 to -48).
  Reset now also: snaps drive/wah/secret-mix smoothers and the pedal base's
  wet/dry and bypass fades (`Pedal::resetBase`); reseeds the LFO and whammy
  spring RNGs; finishes the pickup selector crossfade and a pending
  structural-change fade-in; installs a staged body modal bank; puts each
  string back on its open pitch with the 2 ms glide (a slide left a long glide
  and the old pitch on unplayed, sympathetically ringing strings); resets the
  MIDI controller values expression and pick position, the note-release
  timing and the tone strip ramps; clears the tuning-drift cache. The fret-buzz
  setup is re-derived when the guitar's scale or string count changes (it kept
  the previous guitar's when the setup arrived first). Controllers reset to
  their defaults like the sustain pedal already did; a held expression pedal
  is re-read on its next move.
- **A poly-mode chord sounds one chord window after its first note**, on that
  sample: the window is the latency the interpreter reports, so after host
  compensation it lands where it was played. Before, every chord group started
  on its block's first sample (up to a block early), and a note after a
  group's window had closed still joined it. Found by the live PICK SysEx
  test; `Controllers.chordGroupsSoundOneWindowAfterTheyWerePlayed`.
- **More state a reset now clears**: the coupling matrix's designed pitches
  (it skips moves under 0.5 Hz, so the receive filters kept the last render's
  pitches), a pickup coil's cover EQ, and the tuning-drift RNG. With these the
  first render after prepare matches the second exactly.
- **MIDI OUT's Generic round-trip test runs at 3840 PPQ**: midi-export 12
  checks the content; the tick grid is Generic's by design (3).
- **Live character / noise events are SysEx per trigger**: squeak and pick
  scrape as SQUEAK (trigger shift / drag), a pick click as PICK, fret buzz as
  BUZZ, a slide landing as CLANK. A pick's chirp is the same pluck as its click
  and is not sent twice. Workshop fits go out as WORKSHOP (slot id, fitted,
  was) from a 16-entry lock-free queue, drained every block so switching the
  source on never releases a backlog.
- **Export defaults (Options -> MIDI) are user-global**, in UiPreferences as
  .midprofile JSON: a preset must not change what an export writes. The
  workspace tab is now remembered by name, since tabs are being added in the
  middle of the fixed order.
- **Aux 8 (pick-noise 1.3) is a stereo output declared after the twelve
  per-string buses**, so no bus number a session already uses moves; its
  strip is the eighth aux strip (trim, mute, solo, meter) and counts as aux
  for layout negotiation. The noise sum is mono, sent to both sides. It has
  no latency of its own beyond the per-string taps (under the 128-sample
  allowance). routing-io.md's table still lists seven aux buses; this entry
  is the record of the eighth.
- **Output buses are told apart by their declared names**, not by position.
  The plugin declares every bus and the host disables the unwanted ones, so
  in layout C a string bus sits after seven disabled aux buses: counting from
  bus 1 sent string N's audio to the wrong bus (or nowhere).
  `PluginBuses.perStringLayoutPutsEachStringOnItsOwnBus` drives the real
  processor; the routing harness declares only one layout's buses, which is
  why the old tests passed.
- **Feedback is the physical loop of ambiguity-resolutions 1** (`FeedbackLoop`).
  The amp's output, delayed one block plus distance / 343 m/s, reaches every
  ringing string through a band-pass at its note times 2^octave_bias (focus
  sets Q from 2 to 30), scaled by k_couple = amount x distance (0.5 m / d,
  capped at 4) x directivity (0.25 + 0.75 x (1 + cos angle) / 2) x 0.7 for a
  wound string, and injected at the excitation point, so the pickups, the
  circuit and the volume knob are inside the loop. The injection is linear in
  the amp output (1.1's "gain-scaled by the amp output level" read as the amp
  output carrying its own level: an extra envelope factor would make the loop
  fall at twice the knob's attenuation, against 1.4) and soft-limited with
  tanh at 0.02, so a runaway loop saturates instead of growing. The
  indicator lights at 8% of that ceiling: measured, a loud high-gain rig at
  full amount settles near 19% (the amp saturates first) and a clean amp at
  20% stays under 2%. `kInjectionGain` 0.012 is calibrated by the same test.
  feedback_on / _threshold / _speed stay in the layout for saved automation
  and do nothing; a preset that switched feedback on without an amount loads
  at 50%. Shred Lead uses 45% at 0.8 m.
- **The E-Bow drives each string from itself, through section 1's per-string
  narrowband injection** (`EBowDriver`), not from the amp's output: 2.2 says
  "the feedback path" with a fixed narrowband profile, but an E-Bow sustains
  the same through any amp, and 2.4's "steady within 500 ms at 50%" could not
  hold if it depended on amp gain and the volume knob. Q 30 at the chosen
  partial of the string's note; gain proportional to the shortfall from a
  target level (0.02 + 0.10 x intensity), so it swells and holds. Mask 0 is
  "strings with a held note" (2.2's default): releasing the note lets go;
  explicitly chosen strings keep going after release. A string the E-Bow lets
  go of gets a new `Silenced` damping - an absolute 80 ms T60 - since Choked
  scales with the string's sustain and a long-sustaining note was still at
  -28 dB after 200 ms. `ebow_intensity` is the E-Bow's own parameter rather
  than writing feedback_amount, so using the E-Bow does not switch on amp
  feedback too.
- **The doubler is `PedalType::Doubler`, appended after ParametricEQ** so saved
  slot indices keep their meaning (C-41 resolved in ambiguity-resolutions 3's
  favour). Seven pedal parameters - delay 5-40 (22) ms, pitch +-25 (-8) c, pan
  +-1 (-0.7), width mono / stereo, mix 0-100 (40)%, HP 20-500 (100) Hz, LP
  2-20 (8) kHz; "enable" is the slot's bypass. Mix is a dry / wet crossfade so
  mix 0 nulls (3.2). Each take is a delay with a two-head varispeed for the
  pitch offset, the heads centred on the delay so 0 cents is an exact delay.
  Stereo adds a mirrored take (other side, opposite cents, 1.18x the delay so
  the two do not comb). The old engine doubler (after the cabinet, one voice)
  is gone; `doubler_on` / `doubler_amount` stay for saved automation, and a
  preset with `doubler_on` gets a Doubler in its first free post-amp slot at
  the pedal's defaults, since the old amount meant something different.
- **`Resources/Guitars/migration.json`** (ambiguity-resolutions 7): magic
  `luthier.guitar-migration`, schema 1, version 2, with `renamed` (factory
  files the trademark sweep renamed) and `names` (every guitar-type name ever
  shipped - the pre-sweep ones included - to its .luthierguitar). It is read
  once; `renamedFactoryGuitar` keeps its two hard-coded renames as a fallback
  when the file is not installed. A reference that resolves nowhere loads the
  type's factory guitar with 7's banner text verbatim. The legacy names live
  only in this file and in lines marked for the trademark scan, which now also
  runs in the suite (`Trademarks.sourceTreeHasNoUnmarkedBrandNames`); it had
  six unmarked hits in the retired-preset list that the script, run by hand,
  had not been run to catch.
- **The performance capture is fed from the engine's string activity** until
  the engine reports from triggerNote / applyNoteOff (notation-export 6.1
  wants techniques too): activity has the string and fret the voicer chose
  but not the technique, so captured notes carry no technique marks yet
  (TODO 9). It is clocked per block from the host play-head (6.4) and drained
  by the processor's own 30 Hz timer every third tick (6.2's 10 Hz), which
  also re-sends the tuning when the guitar, tuning or capo changes. Rolling is
  the default state (6.3). The NOTATION tab's scroll speed is how often the
  live tab follows (1, ~3 or 10 Hz; Freeze stops), and MIDI export from it
  goes through the MIDI OUT profile rather than NotationExporter's own
  writer, so there is one MIDI path.
- **Preset morph** (ambiguity-resolutions 5): each side's preset is loaded
  whole (pedal types, structural choices, guitar reference) when the slider
  crosses 0.5, then continuous parameters are interpolated on top, from the
  processor's 30 Hz timer. So discrete, structural and guitar-reference state
  all switch at 0.5 as 5.1 asks, and the ends are the presets exactly (5.3).
  `preset_morph_position` is not stored in presets (a preset carrying it
  would re-trigger a morph on load) and neither are the A / B slots: the
  morph is a performance mode, and turning it on seeds both slots with the
  current sound. The host restores the position as automation.
- **Pedal settings that arrive with a pedal are kept.** The structural path
  wrote a new pedal's defaults over its parameters, which was right for a
  pedal the player picks and wrong for one a preset, snapshot or morph brings
  with its settings - every factory preset's pedals were running at their
  defaults. The bridge listens to the slot parameters and keeps the settings
  when they were written after the type; the preset loader also adopts the
  types directly. Either way the settings are pushed into the new pedal as
  it is built, not one block later.
- **Engine direct MIDI** (tune-builder 8): `LuthierEngine::setDirectMidi`
  takes notes that play as written even while the rhythm engine drives, so
  the TUNE tab's melody, bass and layers are not replaced by the strum
  pattern; its chord part goes through the normal MIDI, where the rhythm
  engine strums it. Rather than merging all of a tune into the host MIDI.
- **TUNE in the plugin** (tune-builder 3.6, 8, 15): the processor owns the
  player and the session. The player renders after the host transport is
  read; while the tune runs on its own clock (host stopped) that clock drives
  the rhythm engine's tempo and position. Its chord channel is merged into
  the host MIDI, after MIDI Learn and live program changes have looked at
  it, so the tune's controllers are never learned; melody, bass and layers
  go through `LuthierEngine::setDirectMidi`. The chord detector reads only
  the host MIDI, so a melody note is never strummed as part of the chord.
  A section's state boundary resets the rhythm engine and the mod matrix's
  envelopes only (not LFOs, sequencers or offsets, which would jump). The
  TUNE tab's Ctrl+S / Ctrl+E / Space act only while the tab has keyboard
  focus: JUCE offers a key to the focused component before the editor, so no
  precedence rule is needed between tune-builder 2 and gui-integration 17.
  The tune keeps its own undo stack (as the report that built it says);
  folding it into the plugin-wide stack is left for action-and-undo work.
- **Where the click goes** (practice-tools 0.2): to the monitor bus by
  default, to the main out when CLICK TO MAIN is on (Practice > Metronome),
  and to the main out regardless when the layout has no monitor bus - the
  standalone app and a stereo-only host layout, where a monitor-only click
  would be silent. The TUNE tab's count-in and metronome follow the same
  route, in the practice metronome's sound and level. The setting is saved
  with the session.
- **PRACTICE in the plugin** (practice-tools 10-12): the processor owns the
  routine runner, the history and the activity tracker, so the drawer that
  advances them and the PRACTICE tab that shows them share one of each. The
  drawer's 20 Hz timer advances the routine and adds minutes; it runs only
  while the drawer is open (0.1), so closing the drawer pauses a running
  routine (and opening it resumes, unless the player had paused it). The
  history is saved every 30 s, on close and when the drawer goes; tests
  point it at a temporary file. A trainer's answers count as one session
  when the player leaves its tab. START on the tab asks the processor for
  the drawer; the editor's timer opens it on the routine's tool, because the
  request can come with no window open. The assistant's resolutions of 11.2
  / 11.3 / 12.1 stand: LOAD, not play, for a saved loop; START IN DRAWER is
  the tab's one transport-like control; one backing-track folder.
- **The session recorder's ring length lives on the PRACTICE tab** (11.2:
  "the settings, not the transport"). The drawer's SESSION tab had its own
  1-60 min slider (default 20) against the tab's setting (default 60, 8's
  figure): two sources of truth. The drawer now shows the length and applies
  the tab's stored setup (defaults.json `session_recorder`) as the recorder
  goes on; the processor never sized the ring otherwise.
- **The rubric voicer replaces ChordVoicer at runtime** (ambiguity-
  resolutions 4). Two instances: the interpreter's places exactly the
  pitches played (exact mode), the rhythm engine's any chord tones in any
  octave in its style (chordTones), each with its own previous voicing for
  4.4's transition bonus. ChordVoicer stays for single notes, chord names
  and the chord library; it is not a fallback, because 4.7 wants
  "unplayable" said. `VoicingStyle::bass` is appended, so saved style
  indices keep their meaning. The rhythm engine's density now caps the
  strings sounded (the voicer applies the style's omissions and caps).
  The voicer's bass-line enum is `RubricBassPattern`, since the tune model
  already has a `BassPattern`; the rhythm engine has no bass-pattern setting
  yet, so Bass voices the root (TODO 2d). A lone note played is not a
  chord: the rubric (4 is "chord auto-fingering") would place it by style
  bias and tie-breaks alone, so it keeps the single-note placement near the
  hand it always had, and becomes the voicing the next chord moves from.
  Found by `EBow.theHarmonicChoiceTakesTheString`, whose E3 moved string.
- **Rubric unisons** (ambiguity-resolutions 4.2): as written, the rubric
  voices C in open style as 8-3-5-0-5-0 (C3 on two strings) rather than open
  C, x32010: muting the low E costs 4, and duplicates cost 1 per pitch class
  whether octaves or unisons. Tried reading hand_span_frets as frets covered
  (3 to 7 for a span of 5) instead of highest minus lowest: the voicer then
  chose 8-7-5-0-5-0 and failed 4.7's own I-IV-V-I travel test, so span stays
  highest minus lowest, as ChordVoicer and rhythm-engine 3 had it, and every
  4.7 test passes. The unison preference is the spec's weights at work, not
  a defect; whether strummed unisons sound right is left to a listening pass
  (TODO 2d).
- **HELP is one surface in two places** (gui-integration 4.4, GAPS A2): the
  column-4 HELP tab in Advanced and the overlay in Easy are the same
  `HelpTab`, as WORKSHOP is. Its shortcut cheat sheet is read from the
  AccessibilitySettings registry, so a rebind shows; it is read-only, and
  rebinding stays in Options (accessibility 9) behind Rebind... Escape and
  the digit keys are listed as fixed rows. include.md's debug button stays
  in Help as well as in Options DIAGNOSTICS; both open the one DebugPanel.
  F1 and the header's ? pin Help to the workspace tab or column section
  holding the focused control - panel-level, because per-control docs do
  not exist (accessibility 2 asks for "the focused control"). A name like
  "String Noise" (column 2 and a CHARACTER group) pins one topic, column 2's.
  No "Take the tour" button (onboarding 2): there is no tour for it to
  start. The help is English only (accessibility 7 pending).
- **String scraping** (string-scraping.md; assistant-built ScrapeEngine).
  Positions are millimetres from the saddle, clamped to the string, so the
  spec's 200-900 mm default range fits any scale; past the fretting finger a
  catch plays at 0.15x. The along-the-string scrape goes into the string's
  excitation input (string-scraping 3, rank above pick-noise 1.2), while the
  pick-noise 5 rake across the strings stays surface noise in PlayingNoise;
  both reach Aux 8. `pick_scrape_amount` trims both, and 0 is silent and free
  (coverage C-29). A string under the slide bar blocks a scrape
  (technique-cascade 2 / 3.4 outrank string-scraping 5's "compatible"). The
  keyswitches (notes 12 scrape, 13 rake down, 14 rake up, below a drop-A bass)
  are recognised in ScrapeEngine::handleMidi ahead of both the rhythm engine
  and the interpreter (input-routing's consumer order), consumed only while
  armed with trigger = Keyswitch; "MPE zone" is channel 16. While a mod-wheel
  or aftertouch sweep runs, vibrato-from-CC is held at 0. Pressure's pitch
  load is a block-rate cents offset, since engine-technique-layer 3.2 keeps
  StringEngine internals closed. The 14 parameters are flat like every other
  (no APVTS group).
- **Strum dynamics** (strum-dynamics.md, ambiguity-resolutions 6;
  assistant-built StrumGesture). Acceleration: the formula as written bunches
  the outer strings, against its own prose and test, so strike times use the
  inverse smoothstep (first and last gaps about 2x the middle at a = 1; total
  time unchanged). Up-strokes are x0.85 force ("softer", no figure given).
  Forces are normalised so the strongest string is the step's dynamic. Misses:
  the leading string at 3x, the others scaled so the mean equals
  strum_miss_probability; misses follow the humanise amount and never touch
  live keyboard chords. Crossing velocity resolves step, then pattern
  (`crossing_sps`, new optional pattern-file fields), then the kit's
  strum_duration_ms (a six-string crossing), then strum_crossing_sps - so
  while a kit is loaded the knob does nothing, and the STRUM group says so
  and offers USE KNOB. A live chord that arrives spread, or over MPE, keeps
  each note's arrival; one that arrives together is strummed at the global
  crossing. strum_speed is superseded but stays declared (automation is
  indexed); presets migrate at 1000 / ms. strum_evenness stays rhythm-engine
  state (kits set it), default now 0.75. A chuck step type is a full chuck;
  chuck_amount blends every strum toward one; StringEngine's Damping::Chuck
  shortens T60 to 10 ms at full. The MIDI chuck key range is deferred (no
  range is specified). Easy mode's Feel scales the crossing and evenness by
  6.3's map; its existing meaning (more humanise to the right) also stays -
  a UX tension left for review.
- **String roll (assistant-built StringRoll.h, on the NOTATION tab above the
  live tab).** The roll reads the performance capture's notes (voiced, drained
  at 10 Hz) for the bars and the engine's per-string level for the right-edge
  glow, so a pluck lights up at the 30 Hz tick rather than a drain later; the
  two sources are never combined into one record. "Now" is the take's newest
  sample carried forward by wall time between drains and only ever re-anchored
  forwards, so late drains cannot make the bars jitter back. The window is four
  bars at the host tempo when the newest note was played to a rolling
  transport (its ppq anchors the bar grid), eight seconds with one-second
  ticks in free play - there is no downbeat to align bars to without a
  transport. Clicking a lane maps its height to frets 0-12 with up = up the
  neck (pitch rises up a piano roll) and its width to velocity 0.35-1.0; the
  capo raises the floor like the fretboard's click. Keyboard and screen-reader
  plucks land on the open string (or the last hovered fret) and a screen
  reader's "press" lets it ring out, since there is no key-up to release on.
  Reduced motion drops the timer to 10 Hz and replaces the eased glow with a
  hard-set marker (accessibility 5's "static colour change"). Collapsing the
  roll (SHOW ROLL, remembered in UiPreferences under `notation.showStringRoll`)
  hands its 120 px to the live tab so the panel's preferred height never
  changes. Strings are plain literals like the rest of NotationPanel and
  FretboardComponent; `tr()` keys for the NOTATION tab are a catalogue pass for
  the whole tab, not this component. The Advanced strip's FRETS | ROLL toggle
  is the lead's (AdvancedPanel); the component is exposed via
  NotationPanel::getStringRoll and constructible on its own for that.
- **Pedal picks build on the message thread they arrive on** (ui-wiring 4.3,
  gui-integration 3.2; "none of the effects work"). A slot type written from
  the message thread - the rack's combo, a reorder, a preset - builds its
  pedal inside the parameter listener, so the rack shows it on its next paint
  and no audio-thread detection / AsyncUpdater round trip stands between the
  click and the sound. Host automation (written on the audio thread) keeps
  the block-rate detection and the async pass. The structural pass is split:
  `readStructuralValues` returns a mask, and a pedal type change runs only
  `applyPedalTypes` - never the instrument pass that reloads body and cabinet
  IRs, re-snaps every string and re-randomises the detune, which a pedal pick
  used to trigger (and which, failing early, skipped the pedals altogether).
  Per-block bypass / mix / knob pushes go through `EffectsChain::applyControls`
  under the swap lock (try-lock; a swap in progress costs one block's push),
  from pointers cached in `cachePointers`, so no parameter ID is built in
  the callback (engine.md 0.2). The `applyStructural` name stays for
  `applyAllNow`, which still runs both passes.
- **The footswitch prints its state.** The LED alone did not say which way
  the switch was, so players "turned every effect on" by stepping on pedals
  that were already on. The face prints ON / BYPASS by the LED (the trademark
  scan covers the words), the switch's tooltip says what a click will do, and
  the type combo says the rack is empty until a pedal is chosen. Easy mode's
  slot popover sizes itself to its pedal when the type changes, so a pedal
  picked into an empty slot gets its whole face instead of the empty slot's
  34 points. These tooltips are literals, as every tooltip in PedalRack and
  EasyPanel is; the `tr()` catalog is used by the widgets and the editor only.
- **Fingers are audibly fingers** (pick-noise.md 6). The flesh-to-nail blend
  was linear in hertz, so the default halfway setting sat a third of an
  octave under a celluloid pick, lost under the cabinet - and measured at
  the string, that gap was under 1 dB above 1.5 kHz, because the string's
  own loop filter and the 1/f^2 pluck shape swamp a contact bandwidth above
  about 2 kHz; what a material sounds like at the string is mostly its
  pulse width (`lengthScale`), which rolls off as sinc^2. The width used to
  scale the comb delay as well, moving the pluck position's notch with the
  material (a thumb plucked at 18% notched as if at 29%); the comb is now
  the position's alone and the width the material's. So the flesh release
  is long - Fingertip 2.2x a celluloid pick's, Thumb 2.6x, the nail's 0.86x
  stands - and nail_vs_flesh blends width and bandwidth in
  octaves with the nail counting as the square of the blend (a short nail
  is still a flesh release), which also makes that control audible for the
  first time. Fingertip's contact bandwidth is 700 Hz with its resonance at
  450 Hz (Thumb 550 Hz; Fingernail's 7 kHz stands), the resonance blends
  with the cutoff, flesh brightens with force at less than half a pick's
  rate, and a finger's contact lengthens as the pluck softens (up to 25% at
  the lightest touch) where a pick's lengthens with thickness and angle.
  Measured at the string (E3, velocity 0.8, half nail), the fingers'
  power-weighted spectral centroid sits about a fifth under the pick's.
  The guitar's own hand - acoustics and classicals
  play with fingers (`applySpec`) - is written into `use_fingers` when the
  player changes guitar type, instead of the engine's default lasting one
  block until the parameter was pushed over it; a preset load keeps the
  preset's own value. Easy's playing strip gets the Fingers toggle beside
  the mode, since it is a by-the-song choice.
- **Pickup type and magnet numbers stay as specified.** part-acoustics.md 6.2
  fixes the magnet table and 6.1 makes position the dominant field, so the
  type / magnet deltas (about 2.5 dB, a 7 -> 4.8 kHz resonance) are left
  alone. What was dead by design is the UI offering pickup 2 and 3 on a
  single-pickup guitar: `LuthierEngine::getNumFittedPickups()` exposes the
  count so the Advanced panel can disable the slots the guitar does not have
  and hide the selector positions it cannot realise.
- **Guitar illustration: detail follows the pixels a millimetre gets.**
  `GuitarRenderer::Options::detail` (full / reduced / thumbnail) replaces the
  `thumbnail` flag; `detailFor (pxPerMm)` picks full from 0.6 px/mm, reduced
  from 0.25, thumbnail below (a 128 px preset thumbnail). Reduced drops what
  is under a pixel at that size - grain, sparkle, dings, purfling, hardware
  shadows, screws, poles' knurl, string shadows and winding dashes - draws
  frets as hairlines and metal as a two-stop gradient. `GuitarBodyComponent`
  gets the level through `buildFitted`, which fits first and rebuilds once if
  the fit's scale (HiDPI counted) asks for another level; the key carries it.
- **The Advanced strip shows the body, not the whole guitar.** 260 x 124 px
  gives a metre of guitar 0.24 px/mm, which is mush. `Framing::automatic`
  crops to the body plus the last five frets (the strip's live parts - pickups,
  knobs, switch - are on the body) whenever the whole fit is under 0.5 px/mm
  and cropping gains at least 10%; the tail sits against the right edge and
  the neck runs off the left. Hit-testing goes through the same transform's
  inverse, so nothing moves; only the frame does. Easy mode (~0.85 px/mm)
  keeps the whole guitar.
- **One soft shadow, blurred at paint time.** The backdrop's two offset fills
  plus a 10 mm stroke, and the per-part `shadow()` pairs, read as hard rings.
  A `Shape` with `shadowMm` is now rendered into a single-channel mask, box
  blurred and drawn in its colour, in pixel space; the backdrop is one such
  shadow under body, neck and headstock together (the neck and headstock no
  longer cast their own). This allocates, so it happens where the scene is
  rasterised into its cache (once per size), never per frame. Diffuse shade
  is 0.12 black at the far edge (was 0.28); the unbound body's edge line is
  the finish's darker shade at 0.3 alpha (was black 0.45, a cartoon outline).
- **A burst is one elliptical gradient plus an inner shadow.** JUCE radial
  gradients are circular, so the fill's transform stretches it to the body's
  proportions (`ellipticalFill`); the edge band that follows the outline is
  an inner shadow (`innerMm`): the mask of everything outside the outline,
  blurred and clipped to the body, in the edge colour. No more stacked rings.
  Thumbnails skip the band. The same stretched-gradient trick keeps the
  lacquer sheen inside its ellipse (it used to end in a hard arc).
- **Grain is a tiled image, not thousands of strokes.** One 256 x 128 px tile
  per figure (straight, tight, open ash, flame, quilt) at 1 px/mm, periodic
  in both directions, built on first use and shared; the body is filled with
  it through a `FillType` offset by the character seed. Downsampling is area
  averaged, so no moire at any size, and it is off entirely below 0.6 px/mm.
- **Carved tops shade as one gradient.** Arched, archtop, semi- and full
  hollow and violin bodies get a crown-to-edge elliptical gradient instead of
  the contour line, which stays for flat tops with a contour.
- **Strings are one gradient stroke** (lit above, shaded below, defined across
  the string's normal so it holds along the fan), never under 1 px; the wood
  shadow only above 0.6 px/mm, winding dashes only at full detail and 2 px.
- **A set neck on a burst keeps its wood**, warmed 30% toward the burst's
  centre colour; it used to take the edge colour and read as a dark slab. A
  set neck under a solid finish is still painted with the body.
- **MIDI import (midi-export 5, tune-builder 9.2 and test 15-08;
  assistant-built `Source/Tune/TuneImport`, TUNE tab IMPORT, File -> Import
  MIDI..., a `.mid` dropped on the window).** The target is the Tune Builder
  only: the session-recorder and looper targets midi-export 5 lists are not
  built, so there is no target choice yet. A file Luthier wrote is recognised
  by its track names (Guitar / Bass / Luthier, or section-named tracks) or
  its `LUTHIER:` technique metas and read back by channel (chords 1, melody
  2, bass 3, layers 4-7; pad / arpeggio / percussion come back as layers, not
  as their generated notes). Any other file is sorted by what its tracks do:
  mostly three-or-more-at-once -> chords, mean pitch under E2 or "bass" in
  the name -> bass, the first monophonic one -> melody, the rest -> the
  countermelody layer verbatim, channel 10 skipped; failing a chord track, a
  chord chart in text metas (parseChordSymbol) is used. Chords are read beat
  by beat through the rhythm engine's ChordDetector, so an import is an
  arrangement, not a playback: the chords are re-strummed by the section's
  pattern and the file's own chord track is kept as a *muted* countermelody
  layer (enabled it would double the strum), and the report says so. Silence
  and unrecognisable stacks hold the chord before them (a cell cannot rest);
  a gap before the first chord moves that chord to the section start, with a
  warning. Markers are sections; a marker whose text and music repeat an
  earlier one folds into a setlist entry (Verse x2 round-trips); no markers
  means one section up to 16 bars, else 8-bar parts; notes before the first
  marker make an Intro. A note across a boundary is cut there and continues
  in the next section (drainRecording truncates; a continued note loses
  nothing). Tempo, time and key signature: the first of each, else 120 /
  4/4 / C major with a warning; SMPTE-timed files are refused by name.
  Imported sections get "Folk Down Up" / "Folk Fingerstyle", the templates'
  pattern and kit. Notes come back locked and from Record; quantise is an
  option (off by default: a sequenced file is already exact). The TUNE tab's
  button reads "IMPORT" (five buttons share the row; "IMPORT MIDI" does not
  fit at 420 pt), accessible name "Import MIDI file". Strings follow their
  neighbours: literals in TunePanel / HeaderBar, `tr` keys for the editor's
  banners. The Export Audio overlay's MIDI-file source now shows the chosen
  file's name in its box.
- **Chords: held strings are out of bounds for the voicer.** engine.md "Mode B"
  says one note per string, but nothing told ChordVoicer / RubricVoicer which
  strings were still sounding, so a note played while others were held took
  its cheapest string and ended the note on it (C4, E4, then G4 killed E4).
  Both voicers take `setOccupiedStrings (mask)`: ChordVoicer skips those
  strings as candidates; the rubric gives an occupied slot a single free
  "leave it ringing" option that is neither fretted nor counted as muted (it
  is not muted - no mute penalty - and a barre across it fails constraint 6,
  which is physically right). MidiInterpreter sets the mask from its held
  slots around every Poly group and the guitar-controller pitch fallback, and
  clears it after; a note that is already held and played again keeps its own
  string (re-pick, as on a guitar) rather than spilling onto a second one.
  Mono mode is untouched: one voice is what it means.
- **Chords: an unfingerable group sounds note by note instead of being
  dropped.** ambiguity-resolutions 4.7's explicit "unplayable" stays the
  voicer's answer, but the interpreter no longer turns it into silence: every
  requested note the chord search did not place (an unplayable group, or a
  voicing that left notes out) is placed by voiceSingleNote on whatever free
  string can sound it, each placed string joining the mask for the next. Only
  a note with no free string left is dropped. The notes still strum as one
  gesture with the rest of the group.
- **Chord window default 2 ms -> 15 ms.** The fingers of a keyboard chord land
  over several milliseconds; at 2 ms they were separate groups, each voiced
  against the last, which is the user's "cannot do chords". 15 ms groups them
  as one strummed chord and is well under the 50 ms latency ceiling the
  integration test allows; the parameter's range (0-20 ms) is unchanged and
  every test that pins a window sets it explicitly.
- **Panic and Reset run on the audio thread's block.** `LuthierEngine::panic`
  and `reset` were called from the message thread with no synchronisation and
  tore the strings, the schedule and the feedback ring mid-render. The
  processor now sets an atomic request that the top of `processBlock` carries
  out before anything reads the engine; with no audio thread running (tests,
  offline renders, the same 200 ms rule ScopedStructuralChange uses) the
  caller carries it out at once. The park mechanism was not used: it fades
  the output over 5 ms each way and waits up to 250 ms, and a panic wants the
  strings choked on the very next block, not a swap.
- **Panic (live-performance 9) now clears every tail.** `LuthierEngine::panic`
  also resets pre/post effects, amp, cabinet, room, master, freeze overlay,
  coupling and the technique state, drops the rhythm engine's held chord and
  pending strums (`RhythmEngine::reset`, enabled state kept), and the
  processor releases the modulation sources and stops the tune player - a
  tune re-feeds its notes every block, so a panic that left it playing was
  undone before it finished. Settings, preset and snapshot are untouched
  (9.5, 9.6).
- **RESET & STOP is the Reset.** `resetEverything` left the tune looping, the
  looper, the backing track, the metronome, a practice routine, the session
  recorder, the rhythm engine's enable / free-run (not parameters: the Easy
  genre box and the PRACTICE drawer set them directly) and the kill switch
  alone, which is the user's loop that "would not stop". `resetAndStop` stops
  all of those first, then restores the parameters, MIDI map, locks and UI
  state, then requests panic + reset on the audio thread; `resetEverything`
  is now that call, one undo step ("Reset and stop"). It is a header button
  beside Panic in the warning colour (label and tooltip in the catalog) with
  Ctrl+Shift+P registered in the shortcut registry, so the HELP tab lists it
  under Playing without further wiring.
- **A chuck mutes every string, and a note-off does not undo it.** The "Chuck
  kills pitch" test found a steady 196 Hz tone at -28 dB after a chuck: not
  the struck strings (all Damping::Chuck, loop gains 0.006-0.26, dead in ~16
  ms) but the open G, which the E-only voicing never strikes. It stayed
  Damping::Open at full sustain and rang sympathetically off the chucked
  strikes through the bridge coupling; the body (bypassed: no change) and the
  idle engine (-77 dB) were ruled out. strum-dynamics 6.1 says every string is
  damped before the strum crosses, so a note-on that carries a chuck now puts
  Damping::Chuck on every other string too (`LuthierEngine::triggerNote`), and
  a note-on without one lifts the hand off the idle strings it was muting
  (Chuck -> Open, sounding strings keep their own damping).
  `StringEngine::release` keeps a Chuck in place instead of replacing it with
  Released (T60 x0.13, hundreds of ms), because the note-offs a chuck step
  sends the strings it does not strike, and the re-strike note-offs from
  `RhythmEngine::emitNote`, land at the same sample as the chuck and would
  have given it its pitch back. Result: clarity 0.21 at -40 dB against the
  strum (was 0.95 at -28 dB), the attack intact, a palm mute still at 0.99.
  What is left in 40-100 ms is a broad -47 dB hump at 185-230 Hz, the solid
  body's bending modes, which is the body resonance the spec says a chuck
  keeps.
- **A cropped frame keeps the tuning popover one click away.** gui-integration
  3.1 opens tuning from the headstock, which the strip's body crop puts off the
  left edge. There, the visible stub of neck (neck, fretboard, strings, nut
  past the body) opens the same popover, its tooltip says the headstock is out
  of frame, and the callout anchors on the visible neck. `Editor.
  everyHitRegionOnTheIllustrationDescribesItself` sweeps a 300 x 620 component,
  which crops too, and finds the headstock text on the neck.
- **"Silent" after Panic / Reset & Stop means the idle floor.** Amp hiss and
  mains hum are deliberate (Engine.silenceInSilenceOut allows a 0.05 idle
  peak), so the ResetStop tests hold the output to three times the idle floor
  measured on an untouched instance, not to zero. The block a panic lands in
  is all zeros; the ~100 ms after it carry the same settle a fresh instance
  has (the engine's filters and noise starting from zero, about -35 dBFS), and
  the tests give it that warm-up before holding it to the floor.
- **Header widths at 1200 px.** The row did not fit gui-integration's 1200 px
  minimum even before RESET & STOP (the preset name had ~20 px). RESET & STOP
  is a two-line button (64 px), the right-hand buttons were trimmed to their
  labels, and below about 1280 the guitar and tuning selectors give way first
  (to floors of 112 / 96 px) so the preset name keeps at least ~70 px.
  Below 1200 the row still truncates from the middle, as it always did.
- **The technique front runs ahead of the scrape's own MIDI handler.**
  engine-technique-layer 3.1 wants one MIDI read for every technique;
  `TechniqueTriggers` (WIP, now integrated) serves the slap, and the scrape
  keeps `ScrapeEngine::handleMidi` behind it rather than being re-plumbed
  through the front in the same step - its keyswitch table entry (12-14) is
  reserved in `TechniqueKeyswitch` so no later technique can claim the keys.
  Moving the scrape onto the front is a follow-up, not a behaviour change.
- **Slap parameters are 426-450, muting 451-458; the count is 458.** The WIP
  header's numbering assumed the strum group's nine had landed (count 425),
  which they had, so the slap's ids are appended exactly as planned and the
  eight MUTE controls follow. The mute grids (the pattern's row and the live
  grid) are not parameters: they travel in `RhythmEngine::toVar` like the
  strum grid, and a pattern step writes `mute_type` only when it is not open,
  so an existing `.luthierpattern` reads and plays exactly as before.
- **Muting is `StringEngine::Damping::Muted`, an absolute T60.** The spec's
  figures (150 / 50 / 20 ms) are times, not scalings of the string's own
  sustain, so a new damping mode takes a T60 and a cutoff directly (like
  Silenced) rather than a scale of the note's sustain (like PalmMute).
  `release()` keeps it, as it keeps a chuck: the note-offs a re-strike sends
  arrive with the strike and Released's T60 (x0.13 of the sustain, hundreds
  of ms) would lengthen the mute, not shorten it. A fret mute is scheduled
  in `LuthierEngine` (80 ms of ring, then Muted at 30 ms); the spec gives no
  figures and those are a classical staccato at any tempo.
- **A palm slap stops the strings with the chuck's damping (amount 1).**
  string-slap 1 says it lands "across muted strings"; over ringing strings it
  is the funk chuck, and Damping::Chuck at 1 is the hand flat on the string.
  The strings are not excited: the event is the hand (a broad thump) and the
  frets (one short clack per string), both from the fret-buzz generator.
- **A scrape starting on a string drops the slap's queued up-stroke on a
  rising edge only.** technique-cascade 2 makes scrape x slap conflict; the
  engine watches each string's scrape activity per block and preempts the
  slap when a scrape *starts*. A level check would also fire while a scrape
  is fading out under the strike that took the string, and drop the strike's
  own rebound.
- **The five HelpContent corrections were never itemised.** TODO 14c and the
  WIP headers name them but no list exists on disk or in the history, so the
  five made are the ones this integration made wrong or incomplete: Getting
  Started and The Interface said six macros (Easy mode has seven, Character
  included) and the Easy playing strip now has the Mute button; RHYTHM gains
  the Mute Row and MUTE group; Options' DIAGNOSTICS line and the Debug Tools
  topic gain Restore first-run experience. The techniques topic does not yet
  list the slap's keyswitches (C0-F#0); that is for the TECHNIQUES tab's
  own text.
- **MuteGroup and SlapGroup are hosted where a tab exists.** gui-techniques
  puts both on the TECHNIQUES tab, which AdvancedPanel does not build yet.
  MuteGroup sits on the RHYTHM tab under STRUM (muting-rhythm 7 puts the
  Mute Row there anyway); SlapGroup is a self-contained component with no
  host yet - the TECHNIQUES tab should mount both when it is built.
- **Muting T60 is measured with the window mean removed.** The string's
  output DC blocker (7 Hz, a 23 ms time constant) leaves a quasi-DC tail
  behind a note stopped in tens of milliseconds; the muted loop itself is
  -40 dB by 40 ms, but the raw RMS envelope read that tail (0.38 dB/ms,
  exactly the blocker's constant) as a 112 ms T60. The test's envelope now
  centres each window; the assertion (40-60 ms) is unchanged. The slap's
  ghost test likewise measures from the strike's onset rather than sample 0:
  the Poly chord window and humanised timing put the note past its fixed
  30 ms attack window, where a peak of 0 made the comparison meaningless.
  The ghost's "no clear pitch" is measured above 20 Hz for the same
  reason: the tail sat at -38 dB at 150 ms with the sub-audio residue in,
  which nobody hears as pitch. The fret mute rings 35 ms and stops in 10 ms
  (was 80 / 30): the engine's default room carried the longer note's tail
  to -16.5 dB at 200-350 ms, and the spec asks for silence there. What was
  left after that (-18.5 dB, unchanged by a shorter ring) was the idle
  strings ringing in sympathy, compressed up by the amp: so a muted note in
  the rock-spread style (muting-rhythm 3, the default) silences the idle
  open strings, as the spare fingers across them do, and a ghost's resting
  hand (bass-techniques 5) lies across the idle strings as well as the
  struck one. The classical fingertip style leaves them ringing; the next
  un-muted note-on lifts either, as it lifts a chuck.
- **FirstRun's locale is asserted where a catalog exists.** `Localisation::
  setLocale` only takes a locale whose catalog is beside the plugin
  (Resources/i18n/<code>.json); none ships in this tree and English is built
  in, so the two FirstRun tests accept "en" as well as the mapped locale. The
  mapping itself (matchShipLocale) is checked exactly.
- **The session recorder honours the PRACTICE tab's setup, and its MIDI is
  an export** (practice-tools 8, 11.2). `SessionRecorder` now has the three
  switches - record audio, record MIDI, auto-save on stop - which
  `SessionRecorderSetup::applyTo` sets with the ring length and
  `applySwitchesTo` sets on their own (they cost nothing, so the tab applies
  them to a running recorder at once; only a changed length reallocates).
  "Neither" is not a setting: it is the recorder being off, so it records
  audio. With audio off the clock still runs and the take keeps its length,
  so a MIDI-only take's notes stay at the samples they were played. The MIDI
  side is a lock-free ring of three-byte channel-voice events sized with the
  audio (60 events a second), replacing a `MidiMessageSequence` that
  allocated under a try-lock on the audio thread; a take's MIDI is built as
  a `MidiPerformance` at the samples against the WAV's first sample and
  written by `MidiProfiles::exportToFile` with the MIDI OUT defaults
  (midi-export 8) and the tempo in force, so a saved take is a
  Luthier-profile file that imports back through the same reader. Files are
  `session-YYYYMMDD-HHMMSS.wav` / `.mid` in `Documents/Luthier/Sessions`
  (a `-2` on a same-second clash); "Auto-save on stop" is the drawer's
  SESSION stop calling `SessionRecorder::stop`, which writes the take when
  the switch is set and nothing otherwise (Panic / Reset & Stop only switch
  it off, deliberately: a panic is not a stop the player meant to keep). A
  MIDI-only take is listed by its `.mid`; a WAV's `.mid` is counted into
  its size instead of listed twice. The processor feeds the recorder every
  block, drawer open or not, because it is switched on in Options as well
  as in the drawer and off (the default) it returns at once; MIDI is
  captured before the audio so a block's events are stamped at the block
  they arrived in.
- **The drawer's Save button is the session recorder's drag-out** (midi-
  export 4.2, "drag from the session recorder's own Save button"). A click
  saves to Sessions; pressing and dragging six pixels writes the take under
  `Sessions/tmp` (the 24-hour sweep's folder) as the WAV and the MIDI, the
  MIDI in the Luthier profile or Generic with Alt as the MIDI OUT tab's
  drag does, and hands both to the host. A drag is not a click, so the
  take is not also saved.
- **The looper's default length is the looper's** (11.2 "Looper: default
  length"). `Looper::setDefaultLengthSeconds` makes the first recording
  close itself at that length - rounded to the bar and at least one bar
  when the metronome quantises, exactly as a pressed close is - so a player
  who knows they want four bars need not hit the button on the bar line;
  zero (the built-in default) leaves the length to the second press, as a
  pedal does. A press before the length is reached still closes early.
  `loopCountInBars` stays a routine entry's count-in, which the runner
  counts.
- **The trainers' range and question count are the trainers'** (11.2
  "Trainers: range, question count"). `ScaleTrainer` and `EarTrainer` take
  a note range and a question count, both off (0-127, unlimited) until the
  PRACTICE tab's defaults set them (40-76 / 20). The scale quiz's right
  answer is the pitch class inside the range - out of it is wrong even in
  the right class, so "play the 5th" is about the neck in front of the
  player - and `getExpectedNote` names one note that answers. The ear
  trainer places every question inside the range when it fits (an octave
  for intervals, a fourteenth for progressions) and as low as the range
  allows when it does not. A complete session makes `nextQuestion` report
  the score instead of asking; the drawer shows that once, and the press
  after it files the session in the history and starts the next.
- **Undo entries and A/B slots carry no tune (action-and-undo 0.5 / 3.9).**
  `captureStateBlock (excludeTune)` writes the state block without the
  `tune` property, and `setStateInformation` leaves the Tune Builder alone
  when the property is absent. Before, undoing a knob went through
  `TuneSession::restoreState`, which is a load boundary that clears the
  tune's own history, so one Ctrl-Z reverted every tune edit and emptied
  the tune's undo. Ctrl-Z still goes to the tune's stack only while the
  TUNE tab has focus (`TunePanel::keyPressed`).
- **The Tune's load boundary stays a wipe, not an undoable entry.** 3.9
  wants a boundary; TuneSession clears its history on `newTune` / `load`.
  With the processor stack no longer restoring the tune, the wipe happens
  only on a real load, which is acceptable and left as built.
- **A boundary is undoable, and undo stops once it has been undone.**
  Section 5: the boundary entry reverses the load; the entries older than
  it are on the far side, so `undo()` refuses when the entry just undone
  is a boundary (`isUndoBlockedByBoundary`) and `undo (true)` - Ctrl+Alt+Z,
  `undoAcrossBoundary` in the registry - crosses with a section-15 banner.
  Ctrl+Y is a separate `redoAlt` action because the registry binds one key
  per action. Boundaries: preset load (header arrows, browser, [ ] keys,
  open / import), family switch (with a section-8 warning banner about
  lost parts), setlist step (`applyCurrentSetlistEntry`, which owns the
  snapshot recall inside it).
- **3.3 vs 3.17: 3.17 wins - Easy/Advanced, Live and Slide mode toggles are
  view state and push nothing.** Slide Mode's header toggle does push
  today (`HeaderBar.cpp`); left as is, it changes a parameter.
- **Gesture entries merge on the parameter id within 200 ms of the previous
  entry's clock (3.1 / 3.2 / 4).** The older entry keeps its before-state
  and takes the new after-value; the description is rebuilt as "Change X
  from A to B" from the parameter's own value text. A drag that pauses
  inside one JUCE gesture is still one entry: JUCE reports one begin/end
  per drag, and splitting on pauses would need per-value timestamps.
- **Snapshot recall, save, rename, colour and delete push entries from the
  processor or the LIVE surfaces (3.7); mod-route create / delete / depth /
  enable / clear go through `LuthierAudioProcessor::addModRoute` and
  friends (3.6); a pedal reorder is one `ScopedUndoAction` (3.13).** The
  matrix and the bank know nothing about undo; the state block carries
  both, so an entry is the whole pre-edit state.
- **"Show Undo Depth" is a UiPreferences key (`diagnostics.showUndoDepth`),
  toggled on Options -> Diagnostics and read by the footer.** It is about
  the window, not the sound or the person.
- **2026-09-24 — Linux packaging (installer.md 3, TODO 16): CPack from the
  same build tree, `LUTHIER_BUILD_PACKAGES=OFF` by default.** The rules live
  in `installer/linux/Packaging.cmake`; the top-level CMakeLists only gains the
  option, so nothing changes for the other targets or for the Windows build.
  Two component sets (`sys_*` for the .deb, `tgz_*` EXCLUDE_FROM_ALL for the
  tarballs) because the two packages want different shapes and install()
  destinations are fixed at configure time.
- **Factory content installs once, to `/opt/Luthier/Resources`, not
  `/usr/share/luthier`.** installer.md 3.1 says `/usr/share/luthier/`, but
  `IrLibrary::searchForResources` looks beside the binary and then in
  `commonApplicationDataDirectory/Luthier/Resources`, which JUCE maps to
  `/opt` on Linux; `/usr/share/luthier` would need a code change in Source/,
  which the packaging pass does not own. The standalone therefore lives at
  `/opt/Luthier/Luthier` with `/usr/bin/luthier` a symlink (JUCE resolves
  `/proc/self/exe`, so the walk starts in `/opt/Luthier`), and the VST3 at
  `/usr/lib/vst3/Luthier.vst3` gets a `Resources` symlink at the bundle's top,
  the same place `luthier_copy_resources` puts the folder in the build tree.
  The user install of the tarball mirrors this under `~/.local/share/luthier`,
  `~/.local/bin` and `~/.vst3`.
- **`.installed_version` (installer.md 6) is written by the package, at
  `/opt/Luthier/.installed_version` (postinst) or beside the tarball install
  (install.sh); the per-user `~/Documents/Luthier/.installed_version` is the
  plugin's to write on first load.** A postinst runs as root and cannot know
  which user's Documents to write, and writing the per-user marker from an
  installer would make `FirstRun::isFirstRun` skip onboarding. FirstRun does
  not read either marker yet (FirstRun.h says so); when it does, the per-user
  one decides first run, and the package one can be compared against the
  running version to show the upgrade banner.
- **Nine MIME types, not installer.md's "six".** file-formats.md 1 owns nine
  Luthier extensions (`.luthierpreset`, `.luthierguitar`, `.luthierpart`,
  `.luthiertune`, `.luthierpattern`, `.luthierset`, `.luthierloop`,
  `.luthiercontent`, `.midprofile`); the six predates the parts model. All
  nine are registered in `installer/linux/luthier.xml` and the desktop
  entry. The association only launches Luthier: the standalone does not open
  a file from argv yet, so installer.md 13's double-click test is pending
  on Source/.
- **Uninstall keeps user data; the tarball's uninstaller removes exactly its
  manifest.** installer.md 0.3 and 13: `install.sh` records every path it
  writes in `install-manifest.txt`, `uninstall.sh` removes those and only
  those (`--purge` adds `~/Documents/Luthier` and `~/.config/Luthier`). The
  .deb's postrm removes only the marker it wrote and refreshes caches; a
  package script does not enter home directories, so `apt purge` equals
  `apt remove` for user data and the docs say so.
- **Standalone-only bundle (installer.md 4) is the same tarball minus the
  VST3 component** (`cpack -G TGZ -D LUTHIER_STANDALONE_ONLY=ON`), not a
  separate install rule set. Portable install (installer.md 9, Windows only)
  has its Linux form in "run in place": the extracted folder runs as is, with
  a relative `Luthier.vst3/Resources` link, but user data still goes to
  `~/Documents/Luthier`.
- **Signing and reproducibility are hooks, not done.** `scripts/release.sh`
  signs when `LUTHIER_SIGNING_KEY` or `LUTHIER_SIGN_HOOK` is set (keys stay
  outside the repo) and otherwise says UNSIGNED in `manifest.json`;
  `SOURCE_DATE_EPOCH` follows the commit, but LTO output and CPack staging
  are not byte-identical between runs (installer.md 0.5 not met).
- **Not applicable on Linux, left to the platform passes:** installer.md 1
  (Windows .exe, registry, Start menu, `/S`), 2 (macOS .pkg, notarisation,
  universal, AU), 3's `.rpm` (best-effort in the spec; no rpmbuild here), 5.2
  delta patches, 7 enterprise policy placement (policy file reader exists;
  packaging it is a per-site matter), 11 `.luthiercontent` packages, 12
  rollback (a hosting matter), and 13's cross-version upgrade / downgrade
  (one version builds at a time; the tarball tests cover re-install over
  itself).
- **Chambering on the feedback loop is a gain on k_couple, 0 to +12 dB.**
  part-acoustics 2.1 says chambering "feeds" the loop's gain without a
  number. `FeedbackLoop::bodyCouplingFromChambering` maps PartAcoustics'
  0..1 feedback column (solid 0.1, chambered 0.25, semi-hollow 0.5, hollow
  0.8) to 0 dB for solid and +12 dB for hollow, linear in dB between (+2.6,
  +6.9), and floors an acoustic's "n/a" 0 at solid rather than below it. A
  hollow top is a soundboard the room drives directly; +12 dB is the
  conservative end of what archtop players describe, and solid stays at
  unity so the existing Shred Lead / Clean Double-Cut calibration (both
  solid) is unchanged. Compiled guitars go through the same table by body
  shape (`chamberingFeedbackForShape`), in `rebuildBodyFromSpec`, with the
  body it belongs to.
- **The capture's chord track reads the detector's held set, with its own
  30 ms stability wait.** notation-export 4 wants the detector's output
  "where it changes". `ChordDetector::advance` is the rhythm engine's, so the
  capture does not call it: `captureChord` watches the held notes after each
  block, waits the detector's burst window after the last change (a strum is
  one chord), then `detect`s and writes a known symbol that differs from the
  last one written, stamped at the block the set last changed. Unknown (too
  few notes, none) is a gap, not a symbol, and the same chord re-struck is
  not a new one. `ChordSymbol::format` renders into the record's 16 bytes
  without a String.
- **BASS_TECH rides the string-activity queue.** The slap classification
  happens inside `LuthierEngine::triggerNote`, after the note-on push, so
  rather than a second queue the engine pushes a `Kind::bassTechnique`
  record onto the same per-block queue (routing-io 6); the capture turns it
  into the event and `MidiOutRouter` skips anything that is not a note. The
  names are midi-export's: ghost, thump (the double thump's up-stroke), pop,
  else slap. The bar (`captureSlideBar`) is read from the slide engine's
  overlay fret and pressure after each block and written on landing, lift, a
  quarter-fret move or a pressure-class change (light under 0.5).
- **A migration backup is a copy, not the spec's move.** file-formats 2
  says the original is "moved" to Backup/<date>/<name>-v<schema>. A load
  does not rewrite the file - the migrated form reaches disk only when the
  user saves - so moving it would empty the browser of the preset just
  loaded. `PresetManager::backupMigratedOriginal` copies it beside the file
  (the user folder's Backup, the same one saves use), once per day per
  original. What counts as a migration: the legacy `format` marker, a missing
  ranges block, retired pickup placements, feedback_on without an amount,
  strum_speed, the engine doubler.
- **Notation export runs on a `juce::Thread` per task with stage-level
  progress.** `NotationExportTask` takes a copy of the score (or the take's
  MidiPerformance for MIDI) and renders and writes on its own thread; the
  renderers are one call each, so progress is per stage (start, rendered,
  written) and cancel is honoured between stages - a cancel after the
  atomic write finds a finished export. Callbacks post to the message thread
  through a weak reference, so a task destroyed with the panel drops them.
- **`RhythmEngine::selectNotesForStyle` is gone.** The rubric voicer takes
  every held note and applies the style's bias and note cap itself
  (ambiguity-resolutions 4), so the engine's own per-style note picker had no
  caller. The Bass style's pattern (root / root-fifth / walking) is now
  `RhythmEngine::setBassPattern`, saved as `bassPattern` in the engine's
  state (absent = root) and handed to the voicer on every revoice.
- **TUNE tab editors (tune-builder 3.2-3.4, 6, 7; TODO 12).** The chord
  pill's popover is a CallOutBox holding `TuneChordEditor` (root, quality,
  bass, beats or hold-to-end, extensions, emphasis, lock); every control is a
  `tune-chord-edit` on the cell, grouped within 200 ms. The qualities offered
  are a fixed list of common spellings resolved through TuneTheory, so every
  one is a chord the rhythm engine detects. A chord cell gained `locked`
  (written as `locked: true`): Reharmonize and the substitution offers leave a
  locked cell as written, the way generators leave a locked note. Dragging a
  section tab reorders the sections *and* sorts the setlist to match, so the
  drag is heard, not just seen (TuneModel's moveSection alone keeps play
  order). Vary (`varySection`) inserts "<name> var" after the section with its
  melody regenerated on the next seed (locked notes kept), its countermelody
  reseeded, its rhythm stepped to the kit's next pattern, and a setlist entry
  after the original's. The piano roll edits one part at a time - MELODY,
  BASS or LAYER (the countermelody) - through `Tune::setPartNotes`, which
  canonicalises and writes to the melody track, a Manual bass, or the
  countermelody layer; a bass in a derived mode is shown grey and a first
  edit turns it Manual with the derived line kept, so nothing the user heard
  vanishes. Draw on empty space; with Draw off, or Shift held, a drag boxes a
  selection; Shift-click adds; a drag on a note moves the selection by rows
  (semitones) and grid steps. Arrow keys nudge by a grid step or a scale
  step (a semitone when chromatic), Shift by a bar or an octave (seven scale
  steps, twelve semitones chromatic), Ctrl+Up/Down by ten of velocity.
  Ctrl+C/X/V go through a roll-wide clipboard and paste at the cursor (where
  the roll was last clicked, snapped). The panel forwards those keys to the
  roll when the panel itself has focus. Velocity is drawn as brightness and
  a lock as a border mark, so neither is colour alone.
- **Kit suggested tempo (tune-builder 2.1).** GenreKit has no tempo field, so
  `getKitSuggestedTempo` reads the kit's name (as the density does): ballad
  72, reggae 76, blues 88, folk 96, funk 104, country 112, bossa 128, jazz
  132, metal 140, bluegrass 150, punk 168, else 120. Choosing a kit sets the
  tune's tempo only while the tempo is untouched - still 120 or the previous
  kit's suggestion - so a tempo the user set is never overwritten.
- **One-screen export (tune-builder 9, DECISIONS C-53).** `TuneExportPanel`
  is an OverlayPanel the editor's host can show (`TunePanel::onShowOverlay`,
  to be wired by the editor; until then the panel opens it in a DialogWindow)
  with four ticked destinations: audio (WAV / AIFF / FLAC - JUCE ships no MP3
  encoder, so 9.1's MP3 is not offered), MIDI, notation, project. Audio reuses
  `AudioExporter`: the offline instance restores the live state, and
  `TuneSession::setRenderIntent` writes `render: true` into that state so the
  restored session plays the tune itself once from the top, no count-in, no
  click, no loop, with the message-thread service run from the render through
  `TunePlayer::setOfflineServiceHook` (rhythm changes at section starts, the
  next improvised pass). The exporter's sequence is a single all-notes-off at
  the tune's end, which fixes the render's length. Stems are the same render
  once per bus, chained on the exporter's completion, through
  `TuneBusRenderProcessor`, which enables every bus on the inner instance and
  copies one output bus to the stereo pair the exporter records. MIDI goes
  through `MidiProfiles::exportToFile` over `buildTunePerformance` (the
  timeline as a MidiPerformance: channel messages at their samples, the bass
  as part 1, a SECTION event per section occurrence), so the Luthier profile
  is available to a tune (C-53 closed); the older Generic-only
  `writeTuneMidiFile` stays for callers that want the plain file.
  `TuneExportTests` renders a tune offline from the state and nulls it
  against live playback (15-07), restores a saved tune with its file from
  the plugin state and plays it (15-10), and checks the state boundary flag
  reaches the processor on the section's first block (8).
- **Ctrl+T is registered** (`newTune`, gui-integration 17) with a catalog
  description. The TUNE panel answers it while focused (the template picker);
  the editor should forward `is ("newTune")` to `header.onNewTune` so it works
  from anywhere - a one-line hook in PluginEditor::keyPressed.
- **Per-string overrides ride on the strings entry** (guitar-workshop.md 3.3).
  `WorkshopGuitar::stringOverrides` serialises as the strings entry's
  `per_string_override` list, the key file-formats.md 3's example already
  names, numbered for people (1 = the high E) with `gauge_in`, `material`
  and `wound` each optional. No list means no overrides, so every guitar file
  written before this loads unchanged and a file with overrides loads on an
  older build minus them; no schema bump. The engine takes the gauge today
  (`DerivedAcoustics::gaugesIn`); the per-string material is computed into
  `DerivedAcoustics::stringMaterials` and waits on an engine hook
  (LuthierEngine reads one `stringMaterial`). The illustration renderer is
  not edited for it: `BenchIllustration` paints an overridden string over the
  scene in its material's colour, from the same section-10 table.
- **Accessory drags are undo entries in real units through a put-back.** The
  pick's position and angle, the slide's slant and the capo's fret are
  parameters, and an undo entry carries the state from before the gesture,
  which the processor captures only at push time. `WorkshopBench::endGesture`
  therefore sets the parameters back to their gesture-start values, pushes the
  entry with the sentence ("Moved capo fret 3 -> 5"), and sets them again. The
  audio thread reads parameters at block start, so a block that begins between
  the two writes plays the old placement once; no gesture notifications reach
  the host. Chosen over pushing at gesture start (the sentence is not known
  then) and over a processor change (not this builder's file).
- **The slide's bench position is the bench's.** slide-technique-controls.md
  (TODO 5b) owns the bar's position sources; until then `WorkshopBench` keeps
  the fret the bench draws the bar at, the slant goes to `slide_slant`, and
  the fitted slide part maps to the engine's `SlideBar` in
  `WorkshopBench::getSlideBar()` for the hook that will take it.
- **The WORKSHOP tab's padlock is the buzz family's.** Part fields have no
  ranges (workshop-ui.md 5); the parameters the bench holds are the setup
  strip's, which PhysicalRange.cpp puts in `RangeFamily::buzz`, and a family
  is the unit of locking. `WorkshopPanel::rangeFamilies()` says so and
  AdvancedPanel builds the tab as a `RangeTabButton` from it.
- **A convolution swap is primed with the note that was playing.** The cabinet
  and body convolvers run with `juce::dsp::Convolution::Latency`, and
  `ConvolutionInstaller` installs a response by pumping silence through them:
  the convolver's latency buffer then holds silence, and its first
  `getLatency()` samples after the swap are zeros - one block of hole with a
  hard edge at each end, under whatever note is sounding, on every cabinet or
  body change (a preset load, a guitar pick, a morph crossing 0.5). It was
  always there; `PresetMorph.aFourSecondSweepDoesNotClick` passed by the luck
  of where the chord's waveform sat when the hole opened, and ac7ecca's 15 ms
  chord window moved the chord 13 ms and turned a 0.08 step into 0.17. The
  suspect, the immediate pedal build on a type pick, was cleared by
  measurement (the step was identical with it disabled), and the strings, the
  pedals and the amp were continuous through the crossing; only the cabinet's
  output went to zero. So `CabinetEngine` and `BodyEngine` keep the last
  16384 samples of what their convolver sees, and a freshly installed
  response is run over that history (output discarded) before the audio
  thread gets it: its first real block is the tail of the note under the new
  response. The sweep's worst step is now 0.074 against A/B alone at 0.050.
  The history is written by the audio thread and read on the loading thread
  without a lock; a torn sample is a rounding error in the primed tail. In a
  host the engine's 5 ms swap fade also covers the timbre change itself; on
  one thread (the tests) that change is the residual step.
- **The feedback volume-knob test holds a moderate note.** The amp at gain 0
  still compresses a little on a velocity-110 attack, so the loop fell
  0.4-0.5 dB less than the circuit and the 0.5 dB tolerance held by luck of
  where the 0.1-0.6 s window met the attack; the 15 ms chord window delayed
  the note 13 ms, put more attack in the window, and 0.41 became 0.51. The
  loop's own ceiling is not it (its RMS is 1e-4 against a 0.02 tanh ceiling,
  and a smaller amount made the gap larger, not smaller), and the pedals are
  bypassed as intended (removing them changed nothing). Measured with the
  onset pinned (chord window 0): velocity 110 0.39 dB, 90 0.25, 70 0.08,
  50 -0.06. The fixture now pins the window and plays velocity 70, which is
  the linear regime the test's comment already claimed; the physics and the
  tolerance are untouched. Note for the product: the 15 ms default window
  delays every note by 15 ms (reported as latency, so a host compensates).
- **The scrape budget is measured in thread CPU time.** `Scrape.
  anActiveScrapeStaysInBudget` was wall-clock, best of five, and read 6.6 ms
  against a 5 ms budget while four cores compiled next to it (3-4 ms quiet).
  It now takes `CLOCK_THREAD_CPUTIME_ID` (GetThreadTimes on Windows), still
  best of five, and reports the wall-clock beside it; time the scheduler
  gives to a neighbour no longer counts, the budget does not move.
- **2026-09-24 — `use_fingers` follows the guitar only on a player's pick.**
  The instrument pass wrote it from the guitar's category on every guitar
  change but the first, so a snapshot, a setlist entry or automation pairing
  an acoustic with a pick came back with fingers, and the write landed on the
  host mid-crossfade. A pick is a parameter gesture (the header selector's
  attachment sends one; a snapshot or automation does not): `HeaderBar`
  listens for the gesture's end and arms `ParameterBridge::
  followGuitarHandOnNextLoad`, which the pass consumes once. Chosen over
  setting the hand in the picker itself because the category comes from the
  guitar the pass loads (a Workshop file), not from a table by type.
- **`EffectsChain::resetFromAudioThread` mirrors `BodyEngine::reset`.** Panic
  and reset run at the top of the audio callback, and `reset()` blocked on
  `swapLock`, which `setSlotType` (a pedal pick, synchronously from the
  bridge's listener) holds on the message thread. A try-lock resets now or
  leaves it pending; `processStereo` carries a pending reset out under its own
  try-lock before it processes, so no pre-panic tail reaches the output.
- **`requestStop` records the request before it tests for an active audio
  thread, and does not park.** A callback the host resumes inside the 200 ms
  idle window takes the request at the top of its block and the caller's
  exchange then finds nothing to run against the render. Parking through
  `beginStructuralChange` keys on the same idle test and does not park a
  thread it cannot see, so it would not have closed the window.
- **A string ringing under the sustain or sostenuto pedal is taken, until a
  group needs it.** `MidiInterpreter` keeps a `ringing` flag past a let-ring
  release and folds it into the occupied mask, so a new note is voiced around
  it rather than onto it; when fewer free strings remain than the group has
  notes, or no free string can sound one, the voicing is redone around the
  held strings only, since a note taking a ringing string beats a note that
  never sounds. The pedal coming up frees them.
- **A marker section rounded to whole bars keeps only its own span; rounded
  down with notes in the cut-off tail it grows a bar.** The rounded bar count
  reached past the next marker (notes duplicated into both sections) or short
  of it (tail notes dropped). `Region` now carries the marker's end, notes and
  chart entries are clipped to it, and a chart from several tracks is sorted
  by time before its entries are read as successive.
- **Rig strip card heights (TODO 2h).** At 1200 x 720 the strip is ~606
  points for six cards; equal shares left the amp's six knobs in one row of
  16-point bodies (the face's one-row layout is width-bound: six knobs across
  ~200 points can never exceed ~32-point rects). `EasyPanel::cardHeights`
  gives each card the height its controls need (guitar 82, racks 62, amp 228,
  cabinet 92, room 82), the amp card's need being two rows of three on the
  face (~44-point knob rects, 30-point bodies); a taller window's surplus
  goes mostly to the amp and the guitar, a shorter one scales every card
  alike and the face falls back to one row on its own. The amp model choice
  moved into the card's title row beside the AMP plate (the plate is its
  label), the cabinet stacks its combos beside a full-height blend knob, and
  the room's size choice lost its duplicate ROOM label.
- **VU meter on the amp face (visual-polish 4).** The proposal offers the
  header or the rig strip; it is on the face instead, where a meter belongs:
  on a head's covering between the name and the vent, on a combo's grille at
  the top right, and nowhere on a face too short for one. The dial is part of
  the cached face; the needle is drawn live over it from the master bus RMS
  (the header meter's source) at the meters' 30 Hz with 300 ms ballistics,
  0 VU = -18 dBFS, scale -20..+3. It greys by the valves' staleness rule.
  Reduced motion (accessibility 5): no easing, a hard set, at 10 Hz.
- **Room light (visual-polish 4)** is on the Easy ROOM card only (the
  Advanced ROOM section is AdvancedPanel's); a static radial pool, reach from
  the size, strength from the wet level plus the Space macro's share, off
  under High contrast.
- **Preset thumbnails (guitar-illustration 15 / 17).** `GuitarThumbnailCache`
  (Overlays.h): one low-priority worker, 200 entries LRU by
  `GuitarRenderer::keyFor` at `Detail::thumbnail` with the palette's texture
  flag; the same guitar in many presets is drawn once. Section 15's
  "128 x 256" is taken as the pixel budget and drawn landscape (256 x 128)
  because the illustration lies headstock-left. Reading a preset file for its
  `guitar` block is disk work, so it is on the worker too; the block is
  resolved against the part library on the message thread (the library is
  not thread-safe to share) as the processor resolves it, minus the
  migration table - a preset whose guitar cannot be found keeps the
  placeholder. A list row never renders: cache hit, or placeholder.
- **Played-note dot (G 14 / 19).** The 60 ms appearance test is in
  `Editor.aPlayedNoteShowsOnTheIllustrationWithinSixtyMilliseconds`, served
  through `juce::Timer::callPendingTimersSynchronously`. The fade rules (dot
  alpha from the string level, no fade under reduced motion) need
  `GuitarRenderer::paintOverlay` / `GuitarBodyComponent`, which this pass did
  not own; not done.
- **2026-09-24 — Focus ring is the look and feel's, with a test hook.**
  qa-polish 4 / accessibility 2: `LuthierLookAndFeel::drawFocusRing` (2 px
  accent over a 1 px dark halo) is drawn by every draw routine when
  `wantsFocusRing (component)` - keyboard focus, or a component a test named
  through `forceFocusRingFor`. The hook exists because a component only holds
  focus with a window peer and the xvfb runner refuses one (`addToDesktop`
  aborted the X server with BadAtom), so the ring is verified through the same
  branch the real focus takes. `LuthierKnob` / `LuthierSlider` now give their
  sliders keyboard focus (a `juce::Slider` does not want it by default);
  toggles and choices already had it. Arrows nudge, Up/Down cycle a choice,
  Return / Space click a toggle - all JUCE's own key handling, now reachable.
- **Right-click item 13 is offered on every control, not "if bound".**
  gui-integration 16's "Show in Options -> Shortcuts (if bound)" has nothing to
  bind to: shortcuts are actions, not parameters. The item opens the table
  (Options -> Accessibility, the page HelpTab's Rebind uses) on every control.
  Item 12 (Automation ID) is the parameter id; choosing it copies the id to
  the clipboard. Item 6 (Assign to macro) is a mod-matrix route from the macro
  source at full depth, ticked when it exists and removed when chosen ticked -
  a macro addresses a parameter only as a source, so that is what "assign" is.
- **Panel menu has no Collapse.** `SectionHeaderExtras` (Widgets) carries the
  `?` and the header right-click menu (Reset panel, Screenshot, Docs) on every
  AdvancedPanel column section, owned by the column through its property set
  so the column needs no new member. Collapse / expand is left out rather than
  offered as a no-op: no section has a collapse API. Screenshot writes a PNG
  to Pictures/Luthier (JUCE has no cross-platform image clipboard). Reset is
  one `ScopedUndoAction` over the section's `LearnTarget` parameter ids, found
  by walking the column's children between this header and the next.
- **Reduced motion: slide bar snaps, noise strip counts at 5 Hz.** The
  fretboard's 80 ms ease becomes `ease = 1.0` under reduced motion (lands and
  lifts without fade); `NoiseEventStrip` paints one named count per class
  and runs its timer at 5 Hz instead of 30, re-arming from `pollNow` because
  the settings' change message is asynchronous.
- **Bypass is silence.** `processBlockBypassed` clears every output bus; JUCE's
  default passes the input through, and the sidechain shares the main
  output's channels.
- **DC null: 20 factory presets sit above -100 dBFS RMS by design.** Measured
  idle output (no MIDI, 1 s after 0.5 s settle, per-channel RMS, the louder
  channel): Octave Fuzz Stoner -24.7 dBFS, Fuzz Face Lead -31.4, Tapping
  Etude -36.4, Single-Cut Crunch -44.4, P-Bass Flatwound -56.6, Violin Bass
  Grind -56.8, Shred Lead -57.3, J-Style Fingerstyle -61.4, Fretless Mwah
  -65.6, Init -67.7, Ambient Swell -68.1, T-Style Country Twang -70.7,
  Rockabilly Slap -71.9, Dry Instrument -74.5, 5-String Low B -74.6, Clean
  Double-Cut Funk -82.3, Semi-Hollow Chime -89.2, Transposing Trem Chords
  -89.3, Surf Reverb -93.1, Jazz Hollowbody -95.8. Amp hiss and hum are voiced
  (Engine::silenceInSilenceOut), and the fuzzes amplify them. `Presets::
  dcNullOnSilentInput` asserts -100 dBFS for every other preset and twice the
  measured floor for these twenty, by name, so a floor that doubles fails.
- **Reported latency omits the master limiter's lookahead (open).**
  `Plugin::reportedLatencyMatchesAnImpulseWithinOneSample` re-amps an impulse
  through the sidechain and finds it 336 samples later where
  `getLatencySamples() - getLatencySamples (AuxBus::di)` says 261: the 75
  sample gap is `MasterBus`'s 1.5 ms lookahead (72 samples at 48 kHz plus the
  onset smear), which `LuthierEngine::getLatencySamples()` does not add. The
  test asserts the spec (within 1 sample) and fails until the engine reports
  it; the fix is a `MasterBus::getLatencySamples()` returning `lookDelay`,
  added in `LuthierEngine::getLatencySamples()` and to every aux bus that runs
  through the master.
- **Amp gain sweep is monotonic to the knee, then eases.** At 1 kHz, 0.1
  input, every model rises without a drop up to its loudest step; past the
  clipping knee the RMS eases back by up to 1.4 dB (Champ, AC30; Deluxe,
  Rectifier, Bogner, Orange 0.3-0.5 dB) as the sag supply settles. The test
  asserts no drop before the knee and at most 2.5 dB after it, since that is
  what a tube amp does. Rectifier and Bogner change only 0.7 dB across the
  sweep at that input: they are saturated from step 1.
- **Tone stack "neutral" is its flattest setting, not 5/5/5.** At 5/5/5 the
  passive stacks are 9.0 (Fender), 6.4 (Marshall), 11.6 (Vox) and 15.1 dB
  (Modern) peak-to-peak across 80 Hz - 5 kHz - the mid dip a real stack has.
  `Amp::toneStackAtNeutralIsFlatAcrossItsPassband` searches an 11-step grid
  per pot and requires the flattest setting to be within 1 dB of flat (2 dB
  peak-to-peak); all four pass, and the test also asserts 5/5/5 is not flat.
- **Overlay focus helper is in Accessibility, not Overlays.** `AccessibleSetup::
  announceOverlayOpened` now finds the first interactive element in Tab order
  through the whole tree (`findFirstInteractive`) and falls back to the
  overlay itself. `OverlayHost::show` (Overlays.cpp:100, another builder's
  file) should replace `current->grabKeyboardFocus()` with
  `AccessibleSetup::announceOverlayOpened (*current, current->getName())`
  once `OverlayPanel` exposes its title (it is `protected`; `getName()` is
  the fallback if the constructor also `setName (title)`).
- **The standalone opens the file it was launched with, and keeps one
  instance.** `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1` (CMakeLists, Luthier
  target) swaps JUCE's `StandaloneFilterApp` for
  `Source/Standalone/StandaloneApp.cpp`: the same window and plugin holder
  (same `~/.config/Luthier.settings`), plus `initialise` passing its argv
  (`filesFromCommandLine`, Support/OpenFile) to the editor's new `openFile`
  on the next message, after the window and the state restore. `openFile`
  dispatches by extension through the existing paths: `.luthierpreset` as
  File > Open (undo boundary, `loadPreset`, `applyAllNow`; a refusal is the
  preset-load banner), `.luthierguitar` via the new
  `LuthierAudioProcessor::loadGuitarFile` (a file in the user/factory guitar
  folders is referenced, one from elsewhere goes whole into
  `guitar.override`; the type follows a factory file, else the family
  template, as `switchGuitarFamily`), `.luthiertune` into the TUNE tab
  (`TunePanel::loadFrom`), `.mid/.midi` via `importMidiIntoTuneBuilder`.
  Anything else, or a missing/unreadable file, is an `open-file` banner.
  Windows/macOS: `moreThanOneInstanceAllowed` false, so a second launch's
  command line reaches `anotherInstanceStarted` (JUCE's WM_COPYDATA
  broadcast / Apple events). Linux: JUCE's `MessageManager::broadcastMessage`
  is an empty TODO, so returning false there would drop the second file;
  the app instead takes its own `InterProcessLock` and a second launch
  writes its absolute paths to `~/.config/Luthier/OpenRequests/*.open`
  (temp + rename) and quits; the running window polls every 400 ms. The
  desktop entry's `Exec=luthier %f` was already right; it now also lists
  `audio/midi` so "Open with Luthier" is offered for MIDI files. The other
  registered types (.luthierpart, .luthierset, .luthierloop, ...) get the
  "cannot open" banner until they have an open path.
- **A piano keyboard plays the guitar and mirrors its strings**
  (`Source/UI/PianoKeyboard.*`). Built on `juce::MidiKeyboardComponent`
  over a *UI-side* `juce::MidiKeyboardState`, whose listener forwards each
  key to the new `LuthierAudioProcessor::triggerPreviewMidiNote` /
  `releasePreviewMidiNote`: a real note-on/off queued into the existing
  `previewMidi` buffer (under `previewLock`, merged in `processBlock`), so the
  normal voicer picks the string just as for host MIDI (the rhythm engine
  also treats it as a held chord, as it does host MIDI). The state is not
  merged on the audio thread with `processNextMidiBuffer`, because that would
  also mark every host note as "pressed". The notes go on channel 16
  (`kPreviewKeyboardChannel`), which no default guitar-controller string map
  uses, so guitar-controller mode voices them by pitch rather than forcing
  string 1. Range: lowest open string (capo'd) to the top playable fret of
  the highest string (`computeGuitarRange`, via `TuningEngine::computeFrequency`),
  padded to white keys, fitted to the width, zoom 1-4x (buttons or
  Ctrl/Cmd + wheel) with JUCE's scroll arrows beyond; middle C = C4.
  Mirroring reads `getStringMidiNote` / `getStringLevel` at 30 Hz (10 under
  reduced motion; visibility-gated like the string roll); a released but
  ringing string keeps its last note and fades with its level (reduced
  motion: on/off). Per-string colours are the palette accent rotated round
  the hue circle (no fixed colours), and every lit key also carries the
  string's number so colour is not the only cue. White/black keys are the
  lighter/darker of `textPrimary` and `background`, so the light palette
  draws paper keys with ink. QWERTY playing is off by default and behind a
  remembered toggle (`pianoKeyboard.qwerty`), because JUCE's A W S E D F T
  G ... map collides with the S, D, T, L and P shortcuts; Left/Right/Up/Down
  and Space/Enter always play it from the keyboard. Placement: the Advanced
  strip toggle is now FRETS | ROLL | KEYS (`AdvancedPanel::StripView`,
  `advanced.stripView` int, the old `advanced.stripShowsRoll` bool still
  read and written); Easy mode has a "Keys" toggle left of the chord readout
  that opens the keyboard (40-72 px) along the bottom of the guitar area
  (`PianoKeyboardDrawer`, `easy.showKeys`).

- **Labels shrink before they are cut; Live Mode's rig strip keeps its racks.**
  "The words in these buttons are not able to be read": column 4's thirteen
  tabs clipped to "RKSH MOD RHYTH ..." and the Workshop bar to "UNERS",
  because `drawButtonText` drew tracked text centred with no fitting and the
  tab-row maths measured the mixed-case label without its tracking. Now
  `Fonts::fitLabel` fits any label: full tracking, then half, then none; then
  a smaller font (never below `Fonts::minimumLabelHeight`, 8.5 pt); then a
  horizontal squeeze to 0.8; and only then an ellipsis. `drawTrackedText`
  fits every label it draws this way, so every panel that uses it (knob,
  choice and slider labels, section plates, tabs) gets it for free. Text
  buttons go through `LuthierLookAndFeel::fitButtonLabel`: normal padding
  (a quarter of the height, 2 - 8 pt), then the 2 pt minimum padding, then
  `fitLabel`; the test measures with the same function. The toggle's label,
  the live strip's pads and set-list triptych, the snapshot grid and the Easy
  rack slots are fitted as well. Column 4's tab strip measures with
  `idealTextButtonWidth` (upper case, tracked, padded), wraps up to seven
  rows, and from three rows uses 24-pt rows so a narrow column keeps its
  workspace; below the widths Advanced allows, the buttons' own fitting takes
  the rest. "Live mode's pre and post effects look squished": Live Mode's
  strip takes 52 pt from the window, and Easy's rig cards all shrank alike, so
  the racks became 3-pt slivers and the guitar card's knobs lost their bodies.
  `EasyPanel::cardHeights` now gives way in order - the amp first, down to its
  one-row face (100), then the guitar, cabinet and room to their floors
  (`floorHeights`) - and the racks keep `kRackMinHeight` (two rows of 20-pt
  slots). A strip shorter than every floor together (`rigFloorHeight`, 456:
  Live Mode with the practice drawer open) scrolls - a scrollbar down its
  right edge, the wheel over the strip - instead of squashing; the controls
  stay children of EasyPanel (offset by the scroll), so the tests' bounds
  checks keep their coordinates. The guitar
  card's knobs sit at the left at their Small width with the response view
  beside them (so they cannot overlap it), and the view hides when it has no
  room. The cabinet card drops the model's own label (the plate says CABINET)
  when two labelled rows do not fit, then the microphones' labels, so every
  combo keeps its full height.

- **Modern Dark palette.** "I also want a 'dark' theme ... something very
  basic that helps you easily view all the controls ... however a modern dark
  mode should look like." A seventh palette, `PaletteId::modernDark`, shown as
  "Modern Dark" in Options -> Appearance, saved and switched live like the
  others and written out as `Resources/Themes/Modern Dark.json`. It is
  appended after Light so saved palette numbers keep their meaning. Values:
  window #161616 (deep #111111), panels #1e1e1e, raised #262626, sunken
  #131313, borders #3a3a3a / #5a5a5a, body text #e6e6e6, secondary #9e9e9e,
  disabled #767676, one blue accent #4c9aff (bright #7db6ff, dim #2d5c99),
  green #3fbf7f / #4cc38a, amber #f5a623, red #f25c54. Body text is 12:1 or
  better on every surface (target 7), secondary text and the accent 5:1 or
  better (target 4.5), disabled text, meter and status colours 3:1 or better;
  `Theme.modernDarkContrastMeetsItsTargets` asserts each pair.
  Modern Dark draws the UI flat: `paletteUsesMaterials()` (Accessibility) is
  false for it and for High contrast, `Palette::apply (colours, id)` sets
  `Palette::textured` from it, and `Palette::usesMaterials()` is the query
  every material site already gates on (panel grain and sheen, corner screws,
  brass section plates and fader caps, lit knob caps, the faces' Tolex, wood,
  grille cloth and metal). The guitar illustration is a picture of a guitar and
  keeps its lighting and real finishes: `Palette::illustrationMaterials`
  (`paletteLightsIllustrations()`, false only under High contrast) drives the
  renderer's `materials` option, and the backdrop behind it is the palette's.
  Three palette roles make the flat look read as a modern dark mode rather
  than High contrast in grey: `Palette::knobBody` is a flat charcoal one step
  above the raised surface (#2e2e2e) so the knob stands off its panel,
  `Palette::knobTrack` (the unfilled value arc) is the bright border so the
  track is plainly visible, and `Palette::meterMid` is the warning amber so
  meters run green, amber, red instead of through the blue accent. In the
  other palettes these roles hold exactly the colours they drew before.
  Table headers (the MOD tab's route list) now take the palette too; they
  were JUCE's pale default in every palette.

## String Detune (2026-09-25)

User request: "slightly detune the strings in either direction, but don't
allow them to completely change the tune of the guitar, just enough to tell
that it is out of tune in either direction."

- **+/-25 cents, and why.** A quarter of a semitone. Against the other strings
  or a tuner it is plainly out - a chord beats several times a second - but it
  is still half the way to the next note, so a guitarist hears the intended
  note played on an out-of-tune guitar, never a different note. The limit is
  `TuningEngine::kMaxStringDetuneCents`; the parameters' range, the engine
  setter and Randomise all use it.
- **Thirteen parameters, 459-471, appended.** `string_detune_1`..`_12` (one per
  string the engine supports, `kMaxStrings`; 1 is the highest string, engine
  string 0, as the nut depths count) in cents, default 0, and `out_of_tune`
  (0..1, default 0). The count is 471. Randomise and Reset are actions that
  write the parameters, not parameters: an automatable button would be a
  trigger that changes twelve other automated values behind the host.
- **Separate from the existing `detuneCents`.** The headstock popover's
  per-string detune (+/-100, engine state, not a parameter) is re-tuning the
  guitar. String Detune is leaving it slightly out, and is a new field,
  `StringTuning::stringDetuneCents`, so neither can cancel the other.
- **Applied on the open string, so it follows the string.** It goes into the
  open frequency before the capo, so every fret, bend, slide and capo path
  (all of which go through `computeFrequency`) carries it, and no other
  string sees it. Pushed every block from the bridge (cached pointers, no ID
  built), so automation moves a ringing note.
- **Clamped together with the string's imperfections.** Realism detune, the
  Drift toggle's walk and the character engine's tuner drift are the string
  being out of tune too; `combineOutOfTune` adds String Detune to their sum
  and limits the total to 25 cents (or to their own size, if they are already
  past it - the control can pull back toward pitch, never further out). With
  String Detune at 0 they pass through untouched. The humanise micro-detune
  is a per-note jitter applied at the note-on, not a tuning, and is left
  alone. The deliberate retune and the fine tuner are tuning, and outside it.
- **The fret lookup ignores it.** `frequencyToFretPosition` works from the
  string without String Detune: a note is fretted where the in-tune guitar
  would fret it and then sounds out. Otherwise the guitar-controller path
  would fret a fraction away and cancel it, and the voicer (8 cents of fret
  slop) would decide a string 25 cents out could not play its own notes.
- **The Out of tune knob.** Turned by hand, it scales the offsets that were
  there when the drag began (keeping a hand-set or randomised pattern's
  shape), or, from in tune, lays down a seeded pattern (the instrument's
  character seed until Randomise rolls another) at the new amount. Randomise
  sets each string uniformly within +/- amount x 25 cents from a seed
  (`StringDetune::randomOffsets`, deterministic per seed). Automating the
  amount alone moves no string: it is the range Randomise uses.
- **Presets.** Saved like every parameter. A preset without the keys (every
  preset from before, and the factory bank) loads with every string at 0 and
  the amount at 0, rather than keeping the last preset's offsets; the patch
  randomiser leaves them alone.
- **Where the controls are.** A STRING DETUNE group in the CHARACTER panel
  under TUNERS: one small knob per string, low string on the left, labelled
  with the string's note from the current tuning and string count (six per
  row, so a 12-string takes two), double-click to zero; the Out of tune knob,
  Randomise and Reset. The Advanced tuning column and Easy's playing strip are
  owned by other panels; the Easy knob is handed to the lead as a snippet.

## Workshop: body swaps, finish paint and a legible drawer (2026-09-25)

- **A body brings its outline.** "Unable to change the body": the body parts
  carried no outline (guitar-illustration.md 4 / 18 say every body part does),
  so fitting one kept the guitar file's `meta.body_style` - the drawing and
  the engine's body shape (`PartAcoustics` shapeFor / baseTypeFor read the
  style) stayed put and only the wood numbers moved. And the drawer listed
  bodies alphabetically with no scrolling, so a guitar's own family's bodies
  were often past the last visible row. Now every factory body part has
  `illustration.body_style`; `WorkshopBench::withPart` sets the guitar's
  body_style from the fitted body (audition too); Revert puts the file's
  style back with the file's body; a family switch that keeps a crossover
  body (the archtop) keeps its style. The renderer draws the guitar's
  body_style first and the part's only when the guitar names none it can
  draw, so a factory guitar that draws a shared body part its own way (the
  7-string's single-cut part as a superstrat) is unchanged.
- **Flamenca Blanca** draws and plays as `flamenco` (was `classical`): its
  body part is the flamenca one, and 4.3's golpeador is on that outline.
- **Wood resonator body.** 4.5: same outline as the steel body, but wood. A
  `metal` finish is drawn as metal only on a steel body (or no body part);
  on a wood body it shows the wood.
- **Another family's part** is fitted with the warning guitar-workshop.md 5
  asks for, never refused: its card reads "Made for acoustic - fits, unusual
  here", and fitting it puts a banner up naming the matched build
  ("Guitar > Acoustic rebuilds around it"). The drawer lists the family's own
  parts first, then the rest; the wheel scrolls it a row at a time, a note
  says how many are out of view, and the fitted part is scrolled into view.
- **Finish paint.** The inspector shows colour chips when the body (its
  colour; a burst's centre and edge) or the plastics (pickguard, knobs,
  selector) are selected, plus six swatches (sunburst, cherry burst, black,
  white, seafoam, natural - 11.1 / 11.2's hexes). A chip opens a
  juce::ColourSelector (saturation/value square, hue strip, typed hex) in a
  call-out; a burst's gets Centre / Edge tabs and a strip of the burst.
  Colours go in the guitar's `finish`: `color_a` (body; a burst's edge),
  `color_b` (a burst's centre) and a new optional `plastic_color` - written
  only when chosen, so older files (and their cache keys) are unchanged and
  an older build ignores it. The plastics colour paints the pickguard, bell /
  top-hat / speed knobs, toggle and blade tips, plastic pickup covers,
  humbucker rings and the whammy tip.
- **Paint is not sound.** `LuthierAudioProcessor::applyEditedGuitar` leaves
  the engine and the parameters alone when the edit differs only in paint
  (`WorkshopGuitar::differsOnlyInPaint`: finish type, colours, burst shape,
  plastics) - no 5 ms structural fade, no refined parameter reset. Gloss and
  aging are not paint (part-acoustics.md 9 / body age) and the picker does
  not touch them.
- **One entry per pick.** A picker drag previews through a bench gesture
  (the bench shows it, nothing is pushed) and commits as one undo entry,
  "Set body colour #7A2E1B -> #B22820", once the drag has rested 450 ms or
  the call-out closes; a swatch is one entry, "Finish seafoam (was ...)".
- **Category bar.** The fourteen drawer tabs take as many balanced rows as
  their natural label widths need (two at the usual bench width, three when
  narrow), so none is clipped whatever the look-and-feel's shrink-to-fit
  does. The inspector's Swap / Revert share a row and "Save as user part"
  has its own.
