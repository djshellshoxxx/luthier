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
