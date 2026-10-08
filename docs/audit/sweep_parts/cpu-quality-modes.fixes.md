No NO-GUI/NO-TEST/PARTIAL/MISSING rows; everything not DONE is OWNED by the cpu-quality-modes FEAT session. Pre-existing code the owner must integrate with:

- NOTE `Source/DSP/Common/Oversampler.h:117 setFactor` calls `reset()` (the click §2.2 removes); `getLatencySamples` table to expose as static `latencyFor`.
- NOTE `Source/DSP/String/StringEngine.h:156 filterDelayCompensation`, `:173 activeDispersionStages = 8` — dispersion cap hook.
- NOTE `Source/DSP/Noise/NoiseEngine.h:186 setDegraded(bool halvePools)` — implemented and exercised in `Source/Tests/NoiseTests.cpp` but never called from `LuthierEngine`; wire it from `applyQuality` for Low.
- NOTE `Source/LuthierEngine.h:428 getCpuEstimate` / `:698 cpuEstimate` (kept for the debug window; `CpuLoadMonitor` becomes the UI source); footer CPU text drawn in `LuthierAudioProcessorEditor::paint` (`Source/PluginEditor.cpp`), replaced by `QualityBadge`.
- NOTE `Source/Accessibility/Accessibility.h:153-159 isReducedMotion/getAnimationMs` — to delegate to `AnimationPolicy::transitionMs`.
- NOTE `oversampling` param (`Parameters.h:271`) controls at `Source/UI/OptionsPages.cpp:554` (AudioPage) and `Source/UI/AdvancedPanel.cpp:945`; `ParameterBridge::applyStructural` (`Parameters.h:507`).
- NOTE Coordinates with animated-strings (`StringAnimator::setReliefLevel` fed via `AnimationPolicy`), output-normalization and preset-browser-previews (both also add `setNonRealtime` handling for `AudioExporter`) — one shared override of `LuthierAudioProcessor::setNonRealtime` needed.
