# Host compatibility

How Luthier behaves in each host, and what we do about each host's quirks
(`spec/host-integration.md` 9). Status: **Handled** = code and a test,
**Works** = nothing special needed, **User** = needs host configuration,
**Open** = not built yet (see `docs/audit/SPEC_SWEEP.md`, host-integration rows).

Add a row whenever a bug bash finds a new quirk.

## Behaviour common to every host

| Topic | Behaviour | Where / test |
|---|---|---|
| State blob | JSON with `stateFormat` (currently 1) and `savedBy`. A newer blob loads what this build understands, warns, and writes back the sections (and a newer preset) it could not read. An older blob is copied to `Diagnostics/StateBackups` before it is migrated. An unreadable blob changes nothing, is kept there too, and a banner says so. | `HostStateFormat.cpp`; `HostState::aNewerBlobKeepsWhatItCannotReadOnWriteBack`, `anOlderBlobIsBackedUpBeforeItIsMigrated`, `anUnreadableBlobChangesNothingAndIsKept` |
| State size | Under 200 KB for every factory preset. Guitars are saved as file references. | `HostState::everyFactoryPresetsSessionIsUnder200KB` |
| Program change after a restore | The first `setCurrentProgram` after `setStateInformation` is ignored, so the restored sound wins. | `PluginProcessor::setCurrentProgram` |
| Internal changes | Preset loads and snapshot recalls call `setValueNotifyingHost`, so the host sees them. | `HostState::presetLoadsAndSnapshotRecallsNotifyTheHost` |
| Transport | When the host is playing, it wins. When it is stopped, the internal clock runs. | `TunePlayer::theHostWinsWhenItPlaysAndTheClockRunsWhenItDoesNot` |
| Sidechain | Used only by a consumer: followers, sidechain-to-amp, tone match. It never leaks to the main output. | `InputRouting::anUnconsumedSidechainNeverReachesTheMainOutput` |
| File dialogs | Each dialog belongs to the window that opened it. Closing the plugin window (or removing the plugin) cancels any dialog that is open. | `OwnedFileChooser`; `ChooserLifetime::*` |

## Per host

| Host | Quirk | Handling | Status |
|---|---|---|---|
| Ableton Live | Re-focuses plugin windows on clip start | JUCE's default window positioning | Works |
| Ableton Live | "Point" automation (single values) | Standard APVTS reads | Works |
| Ableton Live | Rack presets bypass the plugin's presets | Both work side by side | Works |
| Ableton Live | Calls `setCurrentProgram(0)` right after a state restore | That first program change is ignored | Handled |
| Ableton Live | Some versions don't route sidechain to VST3 side inputs | The user sets up the send in Live | User |
| Ableton Live | MPE only from Live 11 | MPE is a controller profile the user picks; auto-detecting it from channel-1-plus-member traffic is not built (HI-37) | Open |
| Logic Pro | Problems with AU states over 1 MB | State stays under 200 KB; guitars are saved as file references | Handled |
| Logic Pro | Bank Select + Program Change combination | PC recalls a snapshot; CC0 picks the preset when "Bank selects preset" is on. A separate PC-mapping-mode control is not built (HI-39) | Open |
| Logic Pro | Re-prepares on preset change | `prepareToPlay` can be called repeatedly (`HostState::aSessionSurvivesThePrepareThatFollowsIt`) | Handled |
| Cubase | VST3 note expression | MPE works; native note expression is a v1.5 target | Open |
| Cubase | Track archive | State size is reasonable | Works |
| Studio One | Filters redundant automation writes | Nothing needed | Works |
| Studio One | Showing and hiding the plugin window | Nothing needed | Works |
| Reaper | Continuous automation | Standard | Works |
| Reaper | JSFX integration | Not a v1 goal | Open |
| Reaper | Multi-output routing | Layouts B, C and D work | Works |
| FL Studio | Wraps VST3 in its own wrapper | Nothing needed for state save / restore | Works |
| FL Studio | Piano roll | Notes arrive correctly | Works |
| Bitwig Studio | Prefers CLAP | CLAP target (`Luthier_CLAP`) when clap-juce-extensions is present | Works |
| Bitwig Studio | The Grid into the sidechain; modulators on any parameter | Standard | Works |
| Pro Tools | Needs AAX | AAX is a v1.5 target | Open |
| Pro Tools | Strict about state size | Same as Logic (under 200 KB) | Handled |
| Standalone | Audio device lost | JUCE's default standalone. Polling and banners are not built (ER-63, HI-41) | Open |
| Standalone | MIDI input lost / virtual MIDI out | Not built (ER-64, HI-41) | Open |
| Standalone | Window resizable, with a minimum size | JUCE window | Works |
| Standalone | File dialogs | OS-native | Works |
