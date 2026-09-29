# Host compatibility

This is the user-facing reference for the host-specific quirks in
[host-integration §9](../spec/host-integration.md#9-host-specific-quirks).
The handling described below is the intended behavior. Host-by-host manual
verification is still required before treating a row as a tested guarantee;
see [the release checklist](RELEASING.md) and [remaining work](audit/REMAINING.md).

## Ableton Live

| Quirk | Luthier behavior or action |
|---|---|
| Clip start may refocus plugin windows | JUCE's default window positioning is used to keep the window stable. |
| Automation may arrive as individual points | APVTS reads each parameter value. |
| Live rack presets and Luthier presets are separate | Either preset system can be used. A rack preset does not become a Luthier preset. |
| Live may send `setCurrentProgram(0)` immediately after restoring state | The first program change after state restore is ignored so the saved sound survives; subsequent program changes work normally. |
| Some Live versions may not route a send to VST3 side inputs | Configure the sidechain send in Live; the plugin cannot force routing. |
| MPE differs across Live versions | Live 11 and later support MPE; Luthier detects channel 1 plus member-channel traffic. Earlier versions may not deliver MPE correctly. |

## Logic Pro

| Quirk | Luthier behavior or action |
|---|---|
| Large AU state can be problematic | State is targeted below 500 KB; GuitarSpec uses a file reference where possible. Keep the referenced guitar file available with the project. |
| Bank Select plus Program Change differs from other hosts | Choose the Program Change mapping mode in the routing panel. |
| Logic may prepare the plugin again after a preset change | `prepareToPlay` is designed to tolerate repeated calls. |

## Cubase

| Quirk | Luthier behavior or action |
|---|---|
| Cubase supports native VST3 note expression | Use MPE input for now. Native note expression is a planned v1.5 feature. |
| Track archives preserve plugin state when it is reasonably sized | Luthier keeps state compact; verify the restored sound when moving an archive between systems. |

## Studio One

| Quirk | Luthier behavior or action |
|---|---|
| Redundant automation writes may be filtered | No special handling is required. |
| Plugin windows can be shown and hidden | Standard window behavior is expected. |

## Reaper

| Quirk | Luthier behavior or action |
|---|---|
| Continuous automation values | Standard parameter handling applies. |
| JSFX integration may be useful | Dedicated JSFX integration is outside the v1 scope. |
| Multiple outputs | Layouts B, C and D are intended to work with Reaper's multi-output routing. |

## FL Studio

| Quirk | Luthier behavior or action |
|---|---|
| The VST3 wrapper owns state save and restore | No special plugin handling is planned. Verify state after reopening a project. |
| Piano roll notes | Standard MIDI note input applies. |

## Bitwig Studio

| Quirk | Luthier behavior or action |
|---|---|
| CLAP is preferred by the host | Current packaged builds include CLAP when the CLAP extensions are available, as well as VST3. The older §9 plan called CLAP a v1.1 feature. |
| The Grid may feed the sidechain | Route The Grid to Luthier's sidechain input. |
| Modulators can target exposed parameters | Choose a Luthier parameter as the modulator destination. |

## Pro Tools

| Quirk | Luthier behavior or action |
|---|---|
| Pro Tools needs AAX for normal plugin support | AAX is planned for v1.5; do not expect the v1 VST3 to load in Pro Tools. |
| Large plugin state can be problematic | The same below-500-KB state target applies as for Logic. |

## Standalone application

| Quirk | Luthier behavior or action |
|---|---|
| Audio device selection and disconnection | Select an audio device in the application. Automatic disconnection polling and recovery are specified in [error recovery §9](../spec/error-recovery.md#9-host-disconnect), but remain unverified. |
| MIDI input and output | Select a MIDI input; the virtual MIDI-out toggle routes to another application. |
| Window size | The JUCE window is resizable with an enforced minimum size. |
| File dialogs | Native operating-system dialogs are used. |
