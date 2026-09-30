## ui-scaling.md

Persisted UI scale (six steps) in ui.json with per-format resize behaviour. Codex-owned lane (no branch found). The feature largely exists under another name: `AccessibilitySettings` holds the scale (accessibility.json, not `ui_scale_pct` in ui.json), with an Options selector and editor `setScaleFactor`. Per-format resize refusal banner and cross-editor broadcast are unverified.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| US-0 (§0) | Ground rules | `Source/Accessibility/Accessibility.h:kScales` | - | `Accessibility::uiScaleStepsAndFontFloor` | PARTIAL |
| US-1 (§1) | Storage: ui_scale_pct in ui.json, nearest-step snap, corrupt fallback | `Accessibility.cpp:AccessibilitySettings::setUiScale` (nearest step); `UiPreferences` owns ui.json | - | `Accessibility::uiScaleStepsAndFontFloor` | PARTIAL |
| US-2 (§2) | Where it is set (Options selector) | `OptionsPages.cpp scaleBox` | Options page, `AccessibilitySettings::setUiScale` | `Accessibility::uiScaleStepsAndFontFloor` | DONE |
| US-3 (§3) | Per-format resize behaviour, refusal banner | `PluginEditor.cpp:setScaleFactor` (line ~44, ~606), `setResizable` | editor | - | PARTIAL |
| US-4 (§4) | Independence from preset/session state | `AccessibilitySettings` global singleton | - | - | NO-TEST |
| US-5 (§5) | Interactions (same-process broadcast, reset to defaults) | `AccessibilitySettings` change notification; `PluginEditor.cpp:605` | Diagnostics reset | - | PARTIAL |
| US-6 (§6) | Failure modes | `UiPreferences` corrupt-file handling | - | - | PARTIAL |
| US-T1 (§7 UISC-01..03) | Round trip, corrupt, off-step snap | - | - | `Accessibility::uiScaleStepsAndFontFloor` (steps only) | NO-TEST |
| US-T2 (§7 UISC-04..07) | New instance parity, broadcast, cross-process, resize refusal | - | - | - | NO-TEST |
| US-T3 (§7 UISC-08..10) | No audio-thread, reset, preset independence | - | - | - | NO-TEST |
