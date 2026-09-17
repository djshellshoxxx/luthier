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

1. **Options > Rescan presets.** This is the fix nine times out of ten.
2. **Check the extension** is exactly `.luthierpreset`. Windows hides extensions by
   default, so `MyPreset.luthierpreset.txt` looks correct in Explorer and is not.
3. **Check the folder.** Options lists every folder being scanned and has a button
   to open each one. User presets belong in:
   - Windows: `%USERPROFILE%\Documents\Luthier\Presets\User`
   - macOS / Linux: `~/Documents/Luthier/Presets/User`
4. **Sub-folders become categories.** A preset in `.../User/Metal/` is filed under
   "Metal". One loose in `.../User/` is filed under "User". Either works.
5. **Add another folder** with Options > Add a preset folder, if you keep presets
   somewhere else - a shared drive, a Dropbox folder, a repository.
6. **Check the file is valid JSON.** Open it in a text editor; it should start with
   `{` and contain `"format": "luthierpreset"`. A truncated file is skipped.

### The factory presets are missing

They are written on first run. If the plugin could not write them - a read-only
install folder, for instance - it falls back to
`Documents/Luthier/Presets/Factory`.

To force a reinstall: **Help > Debug > Reset all settings and clear caches**. This
rewrites the factory bank and does **not** delete your own presets.

---

## No sound

In order:

1. **Is MIDI arriving?** The dot beside the logo lights up when it is. If not, the
   problem is in the host's routing, not in Luthier.
2. **Is the amp on Standby?** The Standby toggle in Advanced mutes the amp, and a
   real amp takes a while to warm back up, so give it a few seconds after turning
   it off.
3. **Is Master up?** Check the master gain in Advanced, and the guitar's own volume
   knob on the illustration.
4. **Are all the pickups off?** On an electric guitar that is silence by design -
   there is nothing to sense the strings. The switch position is drawn on the
   instrument.
5. **Press Panic** (or `0`), in case a note is stuck.
6. **Check the output meter.** If it is moving and you hear nothing, the problem is
   downstream: the track is muted, routed somewhere else, or your interface output
   is wrong.

---

## Crackles, dropouts or high CPU

In order of how much they help:

1. **Lower the oversampling.** Options > Oversampling. 2x sounds very close to 4x
   and costs roughly half.
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
  because a real guitar is never exactly in tune. Set it to zero in Advanced >
  Strings for machine-perfect tuning.
- **Intonation** makes the instrument go progressively sharp up the neck, which
  real guitars do. Set it to zero in the same place.

Also check that **Temperament** is 12-TET, and that **Tuning drift** is off in
Options.

---

## Chords are being voiced strangely

The voicer picks fingerings a hand could actually make, which is not always the
same as the notes you played:

- If a chord needs a fret span wider than a hand, the nearest playable voicing is
  used instead.
- Notes that cannot be reached at all are dropped rather than forced somewhere
  absurd.
- Voicings stay near the previous chord's position, so a progression does not jump
  around the neck.

The fretboard shows what is actually being played. If you want exact control over
which string gets which note, use **Guitar Controller** mode and send each note on
its own MIDI channel.

---

## My timing feels loose

Two causes, both adjustable:

- **Humanize** adds timing jitter on purpose. Turn the macro down, or zero the
  individual Timing control in Advanced.
- **Poly mode strums.** A chord is spread over time rather than triggered at once.
  Set Strum Speed to zero in Advanced for a simultaneous attack.

---

## Notes hang, or a string will not stop

Press **Panic**, or `0`. If it recurs:

- Check for stuck sustain: CC 64 held down by a controller sends "let everything
  ring".
- Check **Freeze** is off - it sustains indefinitely by design.
- Check the **sostenuto** pedal (CC 66).

---

## It sounds thin when I sum to mono

Every factory preset is mono-safe, and there is an automated test for it, but a
patch you build yourself can still cancel. The usual causes:

- **Mic Width** at maximum with two very different microphones.
- **Phase Align** set wrongly. If the two mics are at different distances, their
  time-of-flight difference must be compensated or they comb-filter each other.
- A **very wide chorus or flanger** in the post chain.

Set Mic Width to zero to check. If the thinness goes away, that was it.

---

## The plugin crashes

1. Open **Help > Debug**.
2. Tick **Create log file on crash**. It is off every time the plugin loads, on
   purpose.
3. Reproduce the crash.
4. Reopen the plugin and press **Export troubleshooting file**.
5. Send **both** files, from `Documents/Luthier/Diagnostics`, to
   `support@luthieraudio.example`, with a description of what you were doing.

The two files do different jobs:

- The **troubleshooting file** is a snapshot: your settings, your audio and MIDI
  configuration, your host, the version and a short self-test. Send this first for
  anything that is merely not working as expected. It contains no audio.
- The **crash log** is a running record of what the plugin was doing, with a copy
  of the troubleshooting report at the top. It is only useful for an actual crash.

---

## Last resort

**Help > Debug > Reset all settings and clear caches.**

This resets every setting, clears your MIDI mappings, deletes diagnostic files and
cached data, and reinstalls the factory preset bank. It asks first.

**Your own saved presets are not deleted.**

If that does not fix it, the install itself is probably damaged: delete the plugin
bundle and reinstall.
