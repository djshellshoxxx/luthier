# Troubleshooting

Work down the list. If none of it helps, the last section explains what to send to
support and how to produce it.

---

## The plugin does not appear in my host

**Check it is in the right folder.**

| Format | Location |
|---|---|
| Windows VST3 | `C:\Program Files\Common Files\VST3\Luthier.vst3` |
| macOS VST3 | `~/Library/Audio/Plug-Ins/VST3/Luthier.vst3` |
| macOS AU | `~/Library/Audio/Plug-Ins/Components/Luthier.component` |
| Linux VST3 | `~/.vst3/Luthier.vst3` |

`Luthier.vst3` is a **folder**, not a file. Copy the whole thing, not its contents.

**Rescan.** Most hosts cache their plugin list:

- Ableton Live: Preferences > Plug-Ins > Rescan
- Logic Pro: it rescans at launch; if it fails, Logic > Preferences > Plug-in Manager, then Reset & Rescan
- Reaper: Preferences > Plug-ins > VST > Re-scan, and Clear cache
- FL Studio: Options > Manage plugins > Find more plugins
- Cubase: Studio > VST Plug-in Manager > refresh
- Studio One: Options > Locations > VST Plug-ins > Reset Blocklist
- Bitwig: Settings > Locations > Plug-in Locations > Rescan

**Check it was not blocklisted.** A host that crashed while scanning will refuse to
try again. Every host above has a blocklist or blacklist to clear.

**Check the architecture.** A 64-bit host cannot load a 32-bit plugin. Luthier is
64-bit only.

**On macOS, check quarantine.** Downloaded files get quarantined. In Terminal:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Luthier.vst3
```

---

## Uninstalling by hand

Delete the bundle from the folder in the table above.

Your presets, renders and diagnostics live in `Documents/Luthier` and are **left
alone**, so that reinstalling does not lose your work. Delete that folder too if
you want everything gone.

---

## My presets do not show up

1. **Options > FILE LOCATIONS > Rescan presets.** This is the fix nine times out
   of ten.
2. **Check the extension** is exactly `.luthierpreset`. Windows hides extensions by
   default, so `MyPreset.luthierpreset.txt` looks correct in the file browser and is not.
3. **Check the folder.** Options > FILE LOCATIONS lists every folder being scanned
   and has a button to open each one; the HELP tab's Presets topic shows the exact
   paths on your machine. User presets belong in:
   - Windows: `%USERPROFILE%\Documents\Luthier\Presets\User`
   - macOS / Linux: `~/Documents/Luthier/Presets/User`
4. **Sub-folders become categories.** A preset in `.../User/Metal/` is filed under
   "Metal". One loose in `.../User/` is filed under "User". Either works.
5. **Add another folder** with Options > FILE LOCATIONS > Add a preset folder, if you keep presets
   somewhere else - a shared drive, a Dropbox folder, a repository.
6. **Check the file is valid JSON.** Open it in a text editor; it should start with
   `{` and contain `"format": "luthierpreset"`. A truncated file is skipped.

### The factory presets are missing

They are written on first run. If the plugin could not write them - a read-only
install folder, for instance - it falls back to
`Documents/Luthier/Presets/Factory`.

To force a reinstall: **Help > Open Debug Tools > Reset all settings and clear
caches** (the same button is in Options > DIAGNOSTICS). This
rewrites the factory bank and does **not** delete your own presets.

---

## No sound

In order:

1. **Is MIDI arriving?** The dot beside the logo lights up when it is. If not, the
   problem is in the host's routing, not in Luthier.
2. **Is the amp on Standby?** The Standby switch on the amp's face (Advanced,
   column 3) mutes the amp, and a
   real amp takes a while to warm back up, so give it a few seconds after turning
   it off.
3. **Is Master up?** Check Master in Advanced, column 3, and the guitar's own volume
   knob on the illustration (or in column 2's Circuit section).
4. **Are all the pickups off?** On an electric guitar that is silence by design -
   there is nothing to sense the strings. The switch position is drawn on the
   instrument.
5. **Press Panic** (or `P`, unless you have rebound it), in case a note is stuck.
6. **Check the output meter.** If it is moving and you hear nothing, the problem is
   downstream: the track is muted, routed somewhere else, or your interface output
   is wrong.

### I hear nothing after choosing a pedal

The footswitch on the pedal's face is its bypass, and it reads **ON** or
**BYPASS** so you can see which. A stray click, or a preset saved that way, leaves
it on BYPASS: click the footswitch. Hover it and the tooltip says which way a
click will take it. In Easy mode the same face is in the popover that opens
when you click the slot.

If the footswitch says ON and there is still nothing, check the pedal's own
level or mix controls.

---

## Crackles, dropouts or high CPU

In order of how much they help:

1. **Lower the oversampling.** Options > AUDIO > Oversampling, or the same control
   in Advanced, column 3, under Master. 2x sounds very close to 4x and costs
   roughly half.
2. **Raise your host's buffer size.** 256 or 512 samples is normal for playing;
   1024 or more for mixing.
3. **Empty unused pedal slots.** An empty slot costs nothing, but a loaded one
   costs even when bypassed.
4. **Turn off the second microphone.** It halves the cabinet convolution.
5. **Try the other body mode.** Convolution and Modal have different costs and
   which is cheaper depends on your machine.
6. **Freeze or bounce** finished parts, as with any physical modelling instrument.

The footer shows this instance's CPU share and its reported latency.

---

## The tuning sounds slightly off

That is probably deliberate. Two things do it:

- **Realism Detune** puts each string slightly out of tune and keeps it there,
  because a real guitar is never exactly in tune. It is the **Detune** knob in
  Advanced, column 1, under Tuning Realism; set it to zero for machine-perfect
  tuning.
- **Intonation** makes the instrument go progressively sharp up the neck, which
  real guitars do. Set it to zero in the same place.

Also check that **Temperament** (column 1, at the top) is 12-TET, and that **Tuning
drift** is off - it is the switch under Tuning Realism.

---

## Chords are being voiced strangely

The voicer picks fingerings a hand could actually make, which is not always the
same as the notes you played:

- If a chord needs a fret span wider than a hand, the nearest playable voicing is
  used instead.
- A note the chord search cannot finger is placed on its own, on a free string,
  rather than forced somewhere absurd. Only when no string is free is it dropped.
- Voicings stay near the previous chord's position, so a progression does not jump
  around the neck.
- One note per string. A note played while others are still ringing takes a free
  string instead of cutting one off, so it may land somewhere you did not expect.

The fretboard shows what is actually being played. If you want exact control over
which string gets which note, use **Guitar Controller** mode and send each note on
its own MIDI channel.

### Two notes at once only play one

- **Check the playing mode.** Mono mode is one note at a time by design - the
  second note becomes a hammer-on, pull-off or slide on the same string. For
  chords and intervals use **Poly** (the Mode selector in Easy mode).
- **Check the chord window.** Poly mode gathers notes that land within the
  window - 15 ms by default - into one strummed chord. Set to zero, notes are
  placed one by one as they arrive; that still gives you both notes, but a
  keyboard chord whose fingers land a few milliseconds apart will be voiced as
  separate notes and may not strum. It is in Options > MIDI, and in Advanced
  under Performance.
- **Two notes on one string is impossible**, here as on the instrument. If both
  notes can only be fingered on the same string, one of them has to go to
  another string or be dropped; the fretboard shows which.
- A note played while another rings takes a free string. If every string is
  busy - six notes already held on a six-string - the new one is dropped.

---

## My timing feels loose

Two causes, both adjustable:

- **Humanize** adds timing jitter on purpose. Turn the macro down, or zero the
  individual Timing control in Advanced.
- **Poly mode strums.** A chord is spread over time rather than triggered at once.
  Set Strum Speed to zero in Advanced for a simultaneous attack.

---

## Notes hang, or a string will not stop

Press **Panic**, or `P` (unless you have rebound it). It stops every string and
clears every tail - room, delays, freeze, feedback - and leaves your settings
alone. If it recurs:

- Check for stuck sustain: CC 64 held down by a controller sends "let everything
  ring".
- Check **Freeze** is off - it sustains indefinitely by design.
- Check the **sostenuto** pedal (CC 66).

### A loop will not stop

Panic ends the notes, but something that re-feeds them every block - a tune
playing, the looper, a backing track, the metronome, the rhythm engine
free-running from the Easy genre box or the practice drawer, or the kill
switch held - will have the strings ringing again before Panic has finished.
Press **RESET & STOP** in the header, beside Panic, or `Ctrl + Shift + P`. It
stops all of those, returns every setting to its default and then panics and
resets the engine in one step. It is one undo step: `Ctrl + Z` brings the
settings back once the loop is dead.

---

## It sounds thin when I sum to mono

Every factory preset is mono-safe, and there is an automated test for it, but a
patch you build yourself can still cancel. The usual causes:

- **Mic Width** (the Width knob under Cabinet and Mic) at maximum with two very
  different microphones.
- **Phase Align** set wrongly. If the two mics are at different distances, their
  time-of-flight difference must be compensated or they comb-filter each other.
- A **very wide chorus or flanger** in the post chain.

Set Mic Width to zero to check. If the thinness goes away, that was it.

---

## The guitar picture is tiny or ugly in Advanced mode

The Advanced strip is short, so the illustration is cropped to the body and the
last five frets when the whole guitar would collapse into a smudge, and it is
drawn at a level of detail the size can show - fewer grain strokes, flakes and
dings, not all of them. Nothing has moved: pickups, switch and knobs still
respond, and the visible stub of the neck opens the tuning when the headstock
is out of frame. Easy mode shows the whole guitar at full detail, and a taller
window gives the strip more room. If the strip is not showing the guitar at
all, check the FRETS | ROLL toggle has not swapped the fretboard for the string
roll.

---

## The plugin crashes

1. Open **Help > Open Debug Tools** (`Ctrl + D`), or Options > DIAGNOSTICS.
2. Tick **Create log file on crash**. It is off every time the plugin loads, on
   purpose.
3. Reproduce the crash.
4. Reopen the plugin and press **Export troubleshooting file**.
5. Send **both** files, from `Documents/Luthier/Diagnostics`, to
   `sheldon.davidson@gmail.com`, with a description of what you were doing.

The two files do different jobs:

- The **troubleshooting file** is a snapshot: your settings, your audio and MIDI
  configuration, your host, the version and a short self-test. Send this first for
  anything that is merely not working as expected. It contains no audio.
- The **crash log** is a running record of what the plugin was doing, with a copy
  of the troubleshooting report at the top. It is only useful for an actual crash.

---

## Last resort

**Help > Open Debug Tools > Reset all settings and clear caches**, or the same
button in Options > DIAGNOSTICS.

This resets every setting, clears your MIDI mappings, deletes diagnostic files and
cached data, and reinstalls the factory preset bank. It asks first.

**Your own saved presets are not deleted.**

If that does not fix it, the install itself is probably damaged: delete the plugin
bundle and reinstall.
