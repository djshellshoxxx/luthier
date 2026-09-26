# FACTORY CONTENT SPEC

Every piece of content that ships in the box. Names, categories,
design principles, and what earns a slot.

Design principles first, catalogue second. A shipped file must justify
its slot on the criteria in section 0.

## 0. Ground rules

1. **Every shipped name avoids trademarks.** Reference-style names
   only: "Vintage Single-Cut", "'59 Reissue Alnico 2", "British Stack".
   Legal review before ship.
2. **Tonal spread over depth.** Better to ship 36 presets that cover
   the map than 200 that all sound the same.
3. **Difficulty ladder.** Every category has an easy, a medium, and a
   showcase entry. New user picks up the easy; showcase advertises
   what the plugin can do.
4. **Genre coverage.** Every major guitar genre gets at least two
   presets (rock, blues, jazz, country, folk, classical, metal, funk,
   reggae, latin, indie, ambient, and the same for bass).
5. **Every factory guitar is playable at every factory preset.**
   Loading any preset that doesn't reference a specific guitar picks a
   suitable factory guitar; the sound is good.
6. **Every factory tune loops and sounds finished.** No sketches
   posing as songs.
7. **No copyrighted third-party audio, MIDI, or images.** Ever.
8. **Every content item earns its size.** Total factory content
   budget is 200 MB compressed on disk; anything above that removes
   something else.

## 1. Factory presets (36)

| # | Name | Category | Guitar (default) | Notes |
|---|---|---|---|---|
| 1 | Fresh Strings Clean | Electric / Clean | Vintage Double-Cut | Bright chime, mild compression |
| 2 | Class A Break-Up | Electric / Break-Up | Vintage Double-Cut | 15W combo edge |
| 3 | British Stack Crunch | Electric / Crunch | Vintage Single-Cut | Classic rock rhythm |
| 4 | Modern Overdrive | Electric / Overdrive | Vintage Single-Cut | Default first-run preset |
| 5 | Modern Metal Chug | Electric / Metal | 7-String Modern | Palm-mute grid |
| 6 | Djent Grid | Electric / Metal | 8-String Modern | Extended range |
| 7 | Fuzz Face Wall | Electric / Fuzz | Vintage Double-Cut | Vintage fuzz character |
| 8 | Shoegaze Wash | Electric / Ambient | Offset Modern | Reverb bath, tremolo |
| 9 | Post-Rock Arpeggio | Electric / Ambient | Offset Modern | Long delay swell |
| 10 | Funk 16th Clean | Electric / Funk | Vintage Double-Cut | Compressed, wah-ready |
| 11 | Country Twang | Electric / Country | Classic T-Style | Bright single-coil |
| 12 | Nashville Hot | Electric / Country | Classic T-Style | B-Bender simulation |
| 13 | Blues Break-Up | Electric / Blues | Semi-Hollow 335 | Warm mid drive |
| 14 | Chicago Blues | Electric / Blues | Vintage Single-Cut | Bridge humbucker |
| 15 | Jazz Comping | Electric / Jazz | Full Hollow Archtop | Warm, dark |
| 16 | Jazz Lead | Electric / Jazz | Semi-Hollow 335 | Neck humbucker warmth |
| 17 | Punk Downstroke | Electric / Punk | Vintage Double-Cut | Fast, tight |
| 18 | Indie Jangle | Electric / Indie | Semi-Hollow 335 | Compressed clean |
| 19 | Surf Twang | Electric / Surf | Offset Modern | Spring reverb + trem |
| 20 | Slide Blues | Electric / Slide | Resonator Steel | Glass slide default |
| 21 | Folk Fingerstyle | Acoustic / Folk | Grand Auditorium | Warm mic blend |
| 22 | Bluegrass Flatpick | Acoustic / Bluegrass | Dreadnought | Bright, punchy |
| 23 | Contemporary Fingerstyle | Acoustic / Folk | Grand Auditorium | Modern tapping ready |
| 24 | Delta Blues | Acoustic / Blues | Resonator Steel | Slide-ready |
| 25 | Piedmont Blues | Acoustic / Blues | Parlor | Fingerstyle syncopation |
| 26 | 12-String Chime | Acoustic / Folk | 12-String Jumbo | Doubled octaves |
| 27 | Bossa Nylon | Classical / Bossa | Classical | Warm nylon |
| 28 | Flamenco Rasgueado | Classical / Flamenco | Flamenca Blanca | Bright, percussive |
| 29 | Classical Etude | Classical | Classical | Concert hall room |
| 30 | Gypsy Jazz La Pompe | Acoustic / Jazz | Selmer-Style | Percussive rhythm |
| 31 | Reggae Skank | Electric / Reggae | Vintage Single-Cut | Off-beat comp |
| 32 | Motown Fingerstyle | Bass / Soul | P-Style Bass | Muted flatwound feel |
| 33 | Funk Slap | Bass / Funk | J-Style Bass | Slap-and-pop workflow |
| 34 | Reggae One-Drop | Bass / Reggae | P-Style Bass | Deep, round |
| 35 | Punk Pick Bass | Bass / Punk | P-Style Bass | Pick attack, gritty |
| 36 | Jazz Walking Bass | Bass / Jazz | Full Hollow Bass | Warm upright feel |

Distribution: 20 electric, 6 acoustic, 3 classical, 1 gypsy jazz, 1
reggae guitar, 5 bass. Adjusts as the product grows; this is the ship
list for v1.0.

## 2. Factory guitars (12 `.luthierguitar` files)

Reference names, no trademarks. Every one shipped with a designed
finish, and internally uses distinct part combinations to sound
distinguishable.

**Electric**:
1. **Vintage Single-Cut** — mahogany single-cutaway, carved maple top,
   two humbuckers, 24.75" scale, cherry burst.
2. **Vintage Double-Cut** — alder double-cutaway offset, three
   single-coils, 25.5" scale, sunburst.
3. **Classic T-Style** — ash single-cutaway slab body, single-coil
   neck + single-coil bridge, 25.5" scale, butterscotch blonde.
4. **Semi-Hollow 335** — thin-line semi-hollow, two humbuckers,
   24.75" scale, cherry.
5. **Full Hollow Archtop** — full-depth archtop, one floating
   humbucker, 25.5" scale, sunburst.
6. **Offset Modern** — alder offset double-cut, two mini-humbuckers,
   25.5" scale, seafoam green.
7. **7-String Modern** — mahogany + maple top, two active
   humbuckers, 26.5" scale, satin black.
8. **8-String Modern** — swamp ash multi-scale, two active
   humbuckers, 27"-28" fan, natural oil.

**Acoustic**:
9. **Dreadnought** — spruce top, mahogany back and sides, X-braced,
   natural gloss.
10. **Grand Auditorium** — spruce top, rosewood back and sides,
    X-braced, natural satin.
11. **Parlor** — cedar top, mahogany back and sides, ladder-braced,
    natural.
12. **12-String Jumbo** — spruce top, maple back and sides,
    scalloped X-brace, sunburst.

**Classical**:
- **Classical** — cedar top, rosewood back and sides, fan-braced.
- **Flamenca Blanca** — spruce top, cypress back and sides, thin
  finish.

**Slide-oriented**:
- **Resonator Steel** — steel body, biscuit cone, round-neck.

**Gypsy Jazz**:
- **Selmer-Style** — spruce top, walnut back, oval hole, floating
  bridge.

**Bass**:
- **P-Style Bass** — alder body, split-P pickup, 34" scale, sunburst.
- **J-Style Bass** — alder body, two J-pickups, 34" scale, olympic
  white.
- **Full Hollow Bass** — thin-line hollow, one humbucker, 34"
  scale, sunburst.

Total shipped guitars: 15 (not the 12 in the section header;
"12" was a v0.9 target; the ship list is 15). Correction propagated
in INDEX and onboarding.

## 3. Factory parts (~90 across categories)

Every part is a `.luthierpart` file. Ships as a library user can
mix and match in the Workshop.

**Bodies (12)**: Alder Double-Cut, Alder Offset, Ash T-Slab,
Mahogany Single-Cut, Mahogany SG-Thin, Basswood Superstrat,
Swamp Ash Multi-Scale, Semi-Hollow 335 Body, Full Hollow Archtop
Body, Steel Resonator Body, Wood Resonator Body, Bass P-Style
Body.

**Tops (5)**: Flame Maple, Quilt Maple, Plain Maple, Spalted Maple,
Cedar (acoustic).

**Necks (8)**: Maple Bolt-On C, Maple Bolt-On V, Mahogany Set-Neck,
Maple Set-Neck, Mahogany Thin, Multi-Scale, Bass P-Style Neck,
Bass J-Style Neck.

**Fretboards (5)**: Rosewood, Ebony, Maple, Pau Ferro, Ebony
Bound.

**Fret sets (4)**: Medium-Jumbo Nickel-Silver, Jumbo Stainless,
Vintage Small Nickel-Silver, Fretless (unlined).

**Nuts (4)**: Bone 43mm, Bone 42mm (Vintage), Graphite Locking
42mm, Brass 43mm.

**Bridges (10)**: ABR-1 Tune-o-Matic, Modern TOM, Vintage 6-Point
Trem, Modern 2-Point Trem, Floyd Rose Style, Hardtail String-Thru,
Wraparound, Bass P-Style Bridge, Bass BadAss-Style, Resonator
Biscuit Cone.

**Tailpieces (3)**: Stopbar, Trapeze, Vibrola-Style.

**Tuners (5)**: Kluson 15:1, Modern Sealed 18:1, Locking 21:1,
Vintage Open-Back, Bass 20:1.

**Strings (12)**:
- Electric: 8-38 Extra Light NPS, 9-42 Super Light NPS, 10-46
  Regular NPS, 11-49 Medium NPS, 10-52 Skinny-Top Heavy-Bottom.
- Acoustic: 12-53 Phosphor Bronze, 12-54 80/20 Bronze, 11-52 Silk &
  Steel.
- Classical: Normal Tension Nylon, High Tension Nylon.
- Bass: 45-105 Nickel Roundwound, 45-105 Stainless Roundwound,
  50-105 Flatwound.

**Pickups (18)**:
- Humbuckers: PAF 57 Alnico 2 7.6k, PAF 59 Alnico 5 8.1k, Modern
  High-Output Ceramic 15k, Mini-Humbucker Alnico 5 6.8k,
  Firebird-Style Mini.
- Single Coils: Vintage 54 Alnico 3 5.8k, Modern Noiseless 6.5k,
  T-Style Neck 7.5k, T-Style Bridge 8.5k, P90 Alnico 5 8.2k.
- Active: EMG-Style 81 Active, EMG-Style 60 Active.
- Bass: Split-P Alnico 5 11k, J-Neck Alnico 5 7.5k, J-Bridge
  Alnico 5 8.0k, Music Man-Style Ceramic Humbucker 13k.
- Acoustic: Under-Saddle Piezo, Soundhole Magnetic.

**Wiring / Preamp (8)**: 50s LP Wiring, Modern LP Wiring, Vintage
Strat Wiring, Modern Strat Wiring, T-Style Wiring, Active EMG
Wiring, Bass Passive 2V1T, Bass 3-Band Active Preamp.

**Pickguards (6)**: Cream 3-Ply, Black 3-Ply, White 1-Ply,
Tortoise 3-Ply, Mint Green, None.

**Picks (6)**: Celluloid 0.73 Standard, Nylon 0.60 Standard,
Delrin 1.0 Standard, Ultex 1.5 Sharp, Wooden 3mm Jazz, Thumbpick
Standard.

**Slides (5)**: Glass Delta 22mm, Brass Heavy 25mm, Steel Bar Lap
Steel, Ceramic 20mm, Chrome Standard 22mm.

**Capos (3)**: Trigger Capo, Screw Capo, Partial 3-String Capo.

## 4. Factory tunes (6 `.luthiertune` files)

Each written to showcase a different capability without demanding a
specific taste.

1. **Fingerstyle Etude in Em** — Grand Auditorium, no melody
   overlay; a written arpeggio study over Em-C-G-D.
2. **Late Night in a Minor Key** — Full Hollow Archtop, jazz
   comping under a written melody, Am7-D9-Gmaj7-Cmaj7.
3. **Highway Sketch** — Dreadnought, country strum, C-G-Am-F
   verse, F-C-G chorus, tour-friendly.
4. **Chug Test** — 7-String Modern, drop-A metal riff to show off
   Modern Metal Chug preset.
5. **Delta Slide** — Resonator Steel, Slide Mode on, 12-bar E7
   blues with slide melody.
6. **Funk Slap Groove** — J-Style Bass, funk slap with ghosts,
   16-bar loop over Em7-A7.

## 5. Rhythm patterns (~28 `.luthierpattern` files)

Every genre kit ships with the patterns rhythm-engine.md 7
enumerates: Country Boom-Chick, Delta Shuffle, Piedmont Alt-Bass,
Bossa Comp, Samba, Reggae Skank, Rocksteady, Dub, Funk 16th,
Wah Rhythm, Punk Down, Metal Chug, Djent Grid, Freddie Green,
La Pompe, Rasgueado, Alzapua, Classical Etude, Classical
Arpeggio, Ambient Swell, Post-Rock Arpeggio, plus bass patterns:
Motown Fingerstyle, Funk Slap, Reggae One-Drop, Punk Pick,
Walking Bass, Latin Tumbao, Root-Fifth Country, Dub Bass.

## 6. Genre kits (~28 files)

One `.luthierkit` per genre in section 5. Each bundles the
patterns above with preferred voicer, humanize defaults, pick
style, setup style, and string-noise style.

## 7. Setlists (10 example `.luthierset` files)

1. Solo Acoustic Coffee Shop (10 presets)
2. Rock Cover Set (12)
3. Blues Trio (8)
4. Bossa Cafe (6)
5. Metal Warmup (5)
6. Bluegrass Jam (7)
7. Funk Trio (7)
8. Ambient Loop Show (5)
9. Country Bar Set (10)
10. Jazz Combo (8)

Every entry references only factory presets so the setlist works
on a fresh install.

## 8. Backing tracks (6 WAV files)

Royalty-free, produced for the plugin. Under
`Resources/Practice/BackingTracks/`.

1. 12-Bar Blues in E (120 bpm, 3 min)
2. Funk Groove in D (100 bpm, 4 min)
3. Bossa Nova in Bb (140 bpm, 3 min)
4. Rock Jam in A (140 bpm, 3 min)
5. Country Shuffle in G (110 bpm, 3 min)
6. Ambient Pad in Am (60 bpm, 5 min)

## 9. Example MIDI clips (12 files)

One per genre kit. Under `Resources/Examples/MIDI/`. Each is a 16-
to 32-bar riff or progression suitable for the kit.

## 10. Impulse responses (720)

- 216 body IRs (synthesised, per README): acoustic bodies at
  various sizes, woods, mic positions.
- 504 cabinet IRs: 10 cabinets * 8 speakers * multiple mic
  positions and axes. All generated deterministically by
  `scripts/make_irs.py`.

## 11. Content update packs (post-release)

Shipped as `.luthiercontent` bundles after release. Each pack fits
the same design principles.

Example packs on the roadmap:
- **Vintage Amp Pack 1** — 6 amps, 12 presets, no new guitars.
- **Slide Pack** — 4 guitars (resonator variants), 8 presets, 10
  patterns.
- **Bass Extended Pack** — 5-string and 6-string basses, 4
  presets, 8 patterns.
- **Latin Pack** — 6 acoustic tunes, 4 genre kits, 6 presets.

Each pack goes through the qa-polish gate (section 8 bug bash
included, scaled).

## 12. Naming discipline

- Every user-facing name in the locale catalog.
- Every preset name a noun phrase describing tone or use, not a
  song lyric or a joke.
- Every guitar name follows `[Style Descriptor] [Family Word]`
  ("Vintage Single-Cut", "Semi-Hollow 335").
- Every part name includes the physical fact ("PAF 57 Alnico 2
  7.6k"), not marketing.
- Every tune name is short (< 32 chars).

## 13. Tests

- Load every factory preset in every host: no crash, no missing
  reference banner.
- Every factory guitar renders correctly and matches its
  spectrum-delta fixture within 0.2 dB.
- Every factory tune plays end-to-end without dropouts.
- Every factory setlist loads and every step's preset resolves.
- Every backing track streams at 48 kHz without dropouts.
- Legal review sign-off recorded for every named entry.
- Content-size gate: total factory content <= 200 MB compressed.
- Factory preset previews (preset-browser-previews.md 2): 36 Ogg clips, 3 MB or less, rendered by `scripts/render_previews.sh` at build time and counted in the 200 MB.
