# STRING SCRAPING SPEC

Pick or fingernail drawn along the length of a wound string, producing
the characteristic zipper / ratcheting sound. Common in rock intros,
metal transitions, and creative texturing. Never modelled by sample
libraries convincingly because it's a real physical event: the pick
tip catches each winding of the string in sequence.

## 0. Ground rules

1. **Scraping is a real physical event.** Pick tip catches each
   winding as it travels along the string; each catch is an
   impulsive excitation. No convolution shortcut.
2. **Only wound strings scrape.** Plain strings (high E, B, often G)
   produce almost no scrape noise. The engine models this correctly
   as an emergent effect of winding count.
3. **Scrape speed matters.** Fast scrape (short duration, many
   catches per second) sounds different from slow (individual clicks
   audible).
4. **User controls a scrape gesture.** Not automatic. User triggers
   via keyswitch, MIDI CC, MPE zone, or Playing strip button, plus
   a control that drives the scrape motion.

## 1. Physical model

Every wound string carries `windings_per_mm` (from part-acoustics
string data, typically 4-8 for wound guitar strings, 2-4 for wound
bass). On a scrape gesture:

```
struct ScrapeGesture {
    int string_index;
    double start_position_mm;
    double end_position_mm;
    double duration_ms;
    double pressure;            // 0-1 how hard the pick pushes
    ExcitationTool tool;        // pick, nail, thumb, etc
    double angle_deg;           // pick face angle
};
```

Model:
- Compute travel distance and speed.
- Number of catches = travel_distance * windings_per_mm.
- Each catch is a scaled impulse into the string engine at the current
  position, with amplitude proportional to pressure.
- Catches distributed evenly through the gesture duration.
- Higher pressure produces louder catches AND catches deeper into the
  winding (small pitch modulation as the pick momentarily loads the
  string).

The result is emergent: no scrape "sample", just a stream of
per-winding impulses.

## 2. User controls

Exposed in the Techniques tab (see `gui-techniques-updates.md` 2):

- **Scrape trigger**: keyswitch, CC, MPE zone, or on-screen button.
- **Direction**: bridge-to-nut, nut-to-bridge, or "hold + gesture"
  (a modwheel-style hold and manual sweep).
- **Sweep source**: which real-time control drives the scrape position
  during the gesture. Options: auto (fixed duration), modwheel,
  expression pedal, aftertouch, custom CC.
- **Sweep range**: `start_position_mm` and `end_position_mm`.
  Defaults: 200-900 mm (typical scrape from around the 12th fret
  toward the nut on a standard-scale guitar).
- **Pressure**: 0-1 (default 0.5).
- **Tool**: pick, nail, thumb (default pick).
- **Angle**: pick angle during scrape (default 20°, positive tilts
  toward direction of travel).
- **String mask**: which strings to scrape. Defaults to wound strings
  only (low 3 or 4 depending on set).
- **Retrigger threshold**: minimum time between two triggered scrapes
  (default 200 ms; prevents accidental double-triggers).

## 3. Engine integration

New micro-module `ScrapeEngine`, sits alongside `NoiseEngine` and
consumes from `TechniqueEngine`. Output: stream of impulses fed into
`StringEngine`'s excitation input at scheduled sample offsets.

Interface:
```cpp
class ScrapeEngine {
    void trigger(const ScrapeGesture& g);
    void processBlock(int numSamples); // schedules catches
    void reset();
};
```

Runs at audio rate. Zero cost when idle (single flag check).

Insertion point: in the engine's per-block pipeline, `ScrapeEngine`
runs after `TechniqueEngine` and before `StringEngine`. Impulses
enqueued during the block are applied to the string engine's
excitation buffer at their scheduled sample offsets.

## 4. Presets

- **Classic Rock Scrape**: pick, medium pressure, bridge-to-nut,
  600 ms duration, string mask = low 3.
- **Metal Zipper**: pick, high pressure, bridge-to-nut, 300 ms
  duration, string mask = low 4.
- **Slow Ratchet**: pick, low pressure, 2 s duration, single string.
- **Nail Scrape**: fingernail, low pressure, 800 ms, string mask =
  low 3.
- **Modwheel-Sweep**: hold-and-sweep, modwheel drives position.

## 5. Cascade compatibility

See `technique-cascade.md` for the full matrix. Scrape is compatible
with palm muting (scrape through a mute produces a duller, thumpier
version), slide (rare but possible if the scrape overlaps slide bar
contact - slide takes over damping), and microtonal bends (tension
change slightly shifts winding spacing).

Scrape is not compatible with two-hand tapping on the same string
during the scrape window (tap contact damps the scrape).

## 6. Tests

- Wound low E, 500 ms scrape at pressure 0.5: output shows discrete
  catch impulses at the expected rate (windings_per_mm * speed).
- Plain high E, same gesture: near-silent output (< -30 dB versus
  wound E baseline).
- Pressure doubling: catch amplitude roughly doubles (within 20%),
  slight pitch modulation increases.
- Direction reversal produces mirror-symmetric catch stream.
- Modwheel-driven sweep: catches follow modwheel motion within one
  block of latency.
- CPU: idle cost < 0.05% on mid class; active scrape < 0.5%.
- Retrigger below threshold silently drops.
