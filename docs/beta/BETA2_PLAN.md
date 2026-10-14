# Beta feedback — findings & fix plan (beta2)

Source: beta tester report (2026-10-09) + an 8-area read-only code investigation.
Status of each item below: **BUG** (defect), **GAP** (missing feature), or
**DISCOVERABILITY** (already exists, just hard to find). Effort: small / medium /
large / xlarge. File refs are starting points, not exhaustive.

Pleasant surprises (already built, not missing):
- **Theme switcher already exists** — 6 palettes (Default, 3 colour-blind-safe,
  High-contrast, Light) on Options → APPEARANCE under the heading "THEME AND SIZE".
  Problem is discoverability, not absence.
- **Normalize on/off already exists** — Options → AUDIO, under Oversampling
  (OutputNormalization + LoudnessNormalizer, true-peak ceiling, factory LUFS table).
  Off by default. Discoverability + default, not absence.
- **Guitar finish/colour model already exists** — GuitarFinish (solid/burst/
  metallic/sparkle/transparent/natural, colourA/B, gloss, aging), fully rendered and
  even audible (gloss/aging). Missing only a GUI control + a WorkshopBench write path.
- **Screenshot-capture harness already exists** — ScreenshotTests.cpp drives every
  tab/overlay/Options page to PNG; resources ship as loose files (zero binary bloat).
- **Playable-instrument pieces already exist** — triggerPreviewNote(),
  FretboardComponent (clickable strings/frets), OverlayPanel, setBanjoEgg() voicing.

---

## TIER 1 — usability-blocking bugs (do first)

### T1.1 — GUI oversized on 1080p; can't fit; knobs huge; no scroll; button text overflow  [BUG, medium]
Root cause: the UI-scale cap (`AccessibilitySettings::largestScaleThatFits`,
Accessibility.cpp:466-485) is computed against the **minimum** window (940×560), not
the actual 1200×720. On 1920×1080 it approves 1.75×, so the window is drawn at
2100×1260 — larger than the screen in both axes. Aggravators: aspect ratio is locked
(`setFixedAspectRatio` 1.6667, PluginEditor.cpp:237); nothing clamps `setSize` to the
display; EasyPanel + the top-level layout have no Viewport (they squash/clip instead of
scroll); horizontal scroll exists nowhere; button font is sized from height only
(getTextButtonFont, Theme.cpp:1037) and `drawTrackedText` has no ellipsis below 7.5pt.
Fix:
1. Compute the scale cap against the **actual** default size and subtract OS/host
   chrome; clamp initial `setSize` to the display work area / scale. (PluginEditor.cpp
   :683-684, :240-241; Accessibility.cpp:466-485.)
2. Relax/raise the aspect-lock + lower min height so the window can shrink to fit.
3. Shrink default knob metrics (Theme.h:104-112 knob 48→~40 etc.) + add a "compact"
   density option.
4. Wrap EasyPanel (and overflowing Advanced columns) in the existing ScrollHintViewport
   with both scrollbars; stop forcing Advanced content to viewport width
   (AdvancedPanel.cpp:1742,1818).
5. Button text: bound font by width too + ellipsis fallback; unify toggle/text paths.
Risk: GuiReachability / UiSweep / Appearance tests; the CommandPalette double-scale bug
(CommandPalette.cpp:15) to fix alongside.

### T1.2 — Electric → acoustic still plays the electric sound  [BUG, medium]
Root cause: the header guitar dropdown rebuilds the **instrument** model but **not the
rig**. `writeGuitarParameters` (PluginProcessor.cpp:1259-1331) never writes
ampModel/cabType/cabSpeaker/micType/room, and `applyStructural` (Parameters.cpp
:2485-2506) re-applies the **stale electric amp/cab** from those params, clobbering the
`spec.defaultAmp/defaultCabinet` that `applySpec` just set. The rig-follows-guitar logic
(`FamilyDefaults::applyAmpDefaults`) is wired **only** to the Workshop family switch, not
the header dropdown. So acoustic strings play through the retained electric amp.
Fix: on a **player-initiated** type change (guard: `ParameterBridge::guitarTypeByPlayer`),
call `FamilyDefaults::applyAmpDefaults` and also set cab/speaker/mic from the new spec,
via the existing `writeGuitarParameters` lambda (keeps preset/session/automation recall
safe). Add a Combo/Integration test (mirror FamilySwitch.theAmpFollowsTheFamily…).

### T1.3 — Ground hum intermittent and won't turn off; wants on/off + volume  [BUG, small]
Root cause: the "hum" is the single-coil pickup **mains hum** (PickupEngine.cpp:573-605).
Amount is already an APVTS param (`noise_amp_buzz`, default 0.12 = ON) with GUI knobs, but:
(a) **no on/off toggle** — only dragging the amount to exactly 0 silences it;
(b) **intermittent** because it is gated on single-coil pickup share (humbuckers = 0 hum),
so switching guitars/pickups flips it with no visible control moving;
(c) **"won't turn off"** because the Noise Floor Style "Off" deliberately *leaves hum
unchanged* (RealismStyles.h:31 sentinel −1.0; locked by NoiseFloorTests.cpp:769-778).
Fix: add bool param `noise_amp_buzz_on` (default **true** to preserve goldens) + a toggle
beside each existing amount knob; gate the hum in `PickupEngine::processStrings`; make the
"Off" style also set the toggle false (update the test + tooltip). Keep the amount knob as
the volume control (optionally relabel "Single-coil Hum Level").

---

## TIER 2 — discoverability + the "silent acoustic" level fix (quick, high value)

### T2.1 — Acoustic nearly silent after electric; "normalize should fix this"  [BUG+discoverability, small-medium]
Root cause: AcousticDI is a near-unity clean DI (gainScale 1.05) and acoustic uses
piezo+mic with no coil makeup → ~−43 LUFS vs −13..−19 for an electric; raising amp gain
barely moves a clean DI. The remedy (OutputNormalization) **already exists but is OFF by
default** (NormalizationOptions.cpp:24 `kPrefDefaultEnabled`).
Fix: (a) T1.2 already makes acoustic at least use AcousticDI+room; (b) **default
normalization ON** (one-line flip) so switching guitars is level out of the box — matches
the tester's explicit "normalize all guitars no matter which one you choose"; (c) surface
it (T2.2).

### T2.2 — Options: surface Theme + Normalize; add Help button  [DISCOVERABILITY+GAP, small]
- Theme: rename APPEARANCE heading "THEME AND SIZE" → "THEME", move palette/accent to the
  top, and/or add a dedicated THEME tab re-hosting the existing controls. (No new system.)
- Normalize: lift the existing toggle to the top of AUDIO (above Oversampling); keep one
  source of truth (call the existing OutputNormalization path).
- Help from Options: add `onShowHelp` to OptionsPanel (mirror `onShowDebugWindow`), a Help
  button in the overlay, wired to `performAction("help")`, optionally context-sensitive.

---

## TIER 3 — medium features

### T3.1 — Workshop: colour/finish picker + expand option counts  [GAP+BUG, medium]
- Colour: add `WorkshopBench::setFinish/ setHardwareColour` (undoable, via commit()), then
  a "Finish" category in the drawer (curated named-finish cards for v1: Sunburst, Cherry,
  Natural, Black, Metallic Blue, Sparkle…) and/or a finish inspector (ColourSelector +
  type + burstShape). Renderer already draws all of it; "Follow the guitar" accent updates.
- "Only 4 options" is a **UI limit**, not data: the drawer aborts its card loop on vertical
  overflow (WorkshopPanel.cpp:2113-2114) and has no Viewport. Data has 20 bodies, 19
  pickups, 18 bridges/strings, 15 necks… Fix: wrap the drawer in a Viewport, remove the
  overflow break, rework hit-testing to the scrolled coordinate space.
- v1 keeps gloss/aging out of the control (they affect audio → golden regen) unless asked.

### T3.2 — Playable banjo easter egg (Circuit Drift Labs ×5)  [GAP, medium]
New `BanjoPanel : OverlayPanel` embedding a FretboardComponent (5-string banjo tuning)
that calls `triggerPreviewNote/releasePreviewNote`; `overlayShown()`→`setBanjoEgg(true)`,
`overlayHidden()`→`setBanjoEgg(false)`. Add a "Circuit Drift Labs" label in Help with a
timestamped 5-rapid-click counter → dismiss Help, `showOverlay(&banjoPanel)`. Exclude from
search (D17 no-place-row, like the existing secret). Reconcile with the existing motif
"Dueling Banjos" reveal (shared banjo-voice state). Reuses only Free-available DSP.

---

## TIER 4 — large

### T4.1 — Drums sound bad → realistic synthesis (deterministic, no samples)  [BUG, large]
Everything is a sum of decaying sinusoids (ModalResonatorBank) with no transient and
(for metal) far too few modes. Per piece:
- **Kick**: add a velocity-scaled 2-4 ms beater **click** (band-limited noise 1.5-3.5 kHz,
  ~5-8 ms decay); deepen+accelerate the early pitch drop (finish before JM24's 5 ms
  sample); attenuate the muddy 126-160 Hz modes. (Currently nothing above ~160 Hz.)
- **Snare**: broaden the wires (HPF/shelf ~1.5-2 kHz up to 10-12 kHz, not a single 3.5 kHz
  bandpass); drive them from a **smoothed** head-energy envelope (not the per-sample
  |head| gate that ring-modulates at 200 Hz); add a 1-2 ms broadband crack.
- **Toms**: small noise click; deeper pitch bend; raise rack-tom f0.
- **Hats**: switch to **noise-based** (HPF noise 8-10 kHz, short/long decays) — a hat is
  filtered noise; 32 modes can't be one. Optional few metallic modes for colour.
- **Ride/crash**: hybrid — keep the modal bell/ping (JM27 linearity) + add a dense
  filtered-noise shimmer wash.
Hard constraints: deterministic (RtRandom, one draw/sample, re-seed kick+toms in
reset()/setSeed; the cymbal rng 0xC1A5 isn't currently re-seeded — fix for JM04), rate-
compensated envelopes (JM32), hat choke step <0.05 (JM26), stay under the JM34 CPU budget.
Tests to keep green: JM24-28, JM32, JM34, JM04, JM05, JM37/42/09. No stored golden WAV.

---

## TIER 5 — xlarge, phased (the Help overhaul)

Current help = one flat C array of 49 topics, each body a single plain-text string, rendered
in a read-only TextEditor (no links, no images). HelpTabTests.cpp is a strict gate.
Phases (ordering P0→P1→P3→P2→P4→P5):
- **P0 content model** (medium): replace the flat `body` with sections/subsections + stable
  anchor ids; keep id/title/aliases so the gate passes.
- **P1 rich renderer + interlinking** (large): replace the TextEditor with a Viewport +
  AttributedString/TextLayout renderer; parse `[text](anchor)` + an auto-linker keyword→anchor
  map (effects, echo effect, strings, saturation…); clickable, keyboard-reachable; "Related"
  chips. (Decision: native renderer vs juce::WebBrowserComponent/HTML.)
- **P3 structure** (medium, mostly authoring): section-per-function, subsection-per-feature
  with four slots (what it is / how it works / how to use / what it does NOT do); edition-gate
  Pro-only features.
- **P4 debugging page** (medium, wiring): all data exists — Diagnostics (live ring, troubleshoot
  report, self-test), DebugPanel real-time fields, Telemetry licence state. Add an "Upload to
  Circuit Drift Labs" action (honour opt-in; show payload preview; SupportLinks branding).
- **P2 screenshots with arrows** (xlarge — the big one): extend ScreenshotTests.cpp into a "help
  shots" target emitting base PNG + sidecar JSON of target-control bounds; draw arrows/captions
  from the bounds (live over the image is preferred so they survive UI moves); store under
  Resources/Help (loose files, zero binary bloat; watch total size, WebP/cropped). Ongoing
  maintenance cost (regenerate as UI changes).
- **P5 easter egg**: see T3.2 (the "Circuit Drift Labs ×5" trigger lives here).

---

## Branch / build sequencing
- In-flight release builds (Windows run #8, then Linux, macOS) are pinned to their dispatched
  commits — new fix commits don't disturb them. These fixes are **beta2**.
- Recommended: get beta1 (current gate + packaging) merged/released, then land beta2 fixes.
  Tier 1 + Tier 2 are a fast, high-impact "beta1.1". Tiers 4-5 are their own efforts.
