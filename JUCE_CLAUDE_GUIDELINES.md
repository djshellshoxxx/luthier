# JUCE Development Guidelines for Claude Code

> Drop this file at the root of any JUCE project (or add it to `CLAUDE.md` / your project instructions). Claude Code must read this file at the start of every session and follow it before writing or modifying any C++ / CMake code in a JUCE project.

---

## 1. MANDATORY: Use context7 MCP Before Writing JUCE Code

Claude Code's training data on JUCE is often stale. The JUCE API changes between releases (module layout, `AudioProcessorValueTreeState` behaviour, `dsp::` module, CMake helpers, GUI classes, unit test framework, etc). Do not guess from memory.

### Hard rules

1. **Before writing, refactoring, or reviewing ANY JUCE code**, query the context7 MCP server for the specific JUCE class, module, or function you are about to touch.
2. **Before answering an API question** (signatures, defaults, deprecations, template parameters), query context7 first. Never answer from memory.
3. **Before generating a `CMakeLists.txt`** that uses `juce_add_plugin`, `juce_add_module`, `juce_generate_juce_header`, or any other JUCE CMake helper, query context7 for the current signature and available flags.
4. If context7 returns nothing useful for the exact symbol, **say so explicitly** and fall back to `https://docs.juce.com/master/` via web fetch. Do not invent an API.
5. When multiple JUCE versions are relevant, **check the project's actual JUCE version first** (look for `JUCE/CMakeLists.txt`, submodule ref, or `JUCE_MAJOR_VERSION` in `juce_core/juce_core.h`) and query context7 for that version.

### How to call context7

Use the context7 MCP tools in this order for every JUCE lookup:

1. `resolve-library-id` with `libraryName: "juce"` to get the canonical library ID.
2. `get-library-docs` with that ID plus a specific `topic` (e.g. `"AudioProcessorValueTreeState"`, `"dsp::ProcessorChain"`, `"juce_add_plugin"`, `"AudioProcessor::processBlock"`).
3. Prefer narrow topics over dumping the whole library. Set `tokens` only as high as needed.

### When context7 is required (non-exhaustive)

- Any `AudioProcessor`, `AudioProcessorEditor`, `AudioProcessorValueTreeState`, or `AudioProcessorGraph` change.
- Anything in `juce::dsp::` (ProcessorChain, IIR, FIR, Oversampling, Convolution, StateVariableTPTFilter, etc).
- `MidiBuffer`, `MidiMessage`, `MidiKeyboardState`, `Synthesiser`, `SynthesiserVoice`.
- GUI classes: `Component`, `Slider`, `Button`, `LookAndFeel_V4`, `OpenGLContext`, `Graphics`, `Path`.
- Threading primitives: `AbstractFifo`, `SpinLock`, `WaitableEvent`, `Thread`, `TimeSliceThread`, `MessageManager`.
- CMake API: `juce_add_plugin`, `juce_add_console_app`, `juce_add_gui_app`, `juce_add_module`, `juce_add_binary_data`, target link flags.
- Unit tests (`juce::UnitTest`, `UnitTestRunner`).

### If a search is skipped

If Claude Code writes JUCE code without first querying context7, that is a workflow failure. Stop, query context7, and revise the code.

---

## 2. Core Realtime Rules (The Golden Rule)

**Never allocate, lock, or block on the audio thread.** Every rule below flows from that.

Inside `processBlock`, `renderNextBlock`, `getNextAudioBlock`, or any callback the audio driver reaches:

- No `new` / `delete`, no `malloc`, no `std::make_unique`, no `std::make_shared`.
- No `std::vector::push_back`, `resize`, `emplace_back`, `insert`, or any container operation that can allocate. Pre-size in `prepareToPlay`.
- No `std::string` construction, no `juce::String` concatenation, no `printf` / `DBG` in release paths.
- No `std::mutex`, `std::lock_guard`, `juce::CriticalSection`, or `ScopedLock`. Use lock-free structures.
- No file I/O, no network, no `std::this_thread::sleep_for`.
- No calls into the GUI (`Component::repaint`, `setText`, `setValue`) directly. Marshal to the message thread.
- No `dynamic_cast`, no exceptions thrown or caught.
- No `std::function` assignment (may allocate). Assign once, off the audio thread.
- No RNG that locks (`std::random_device` on some platforms). Use `juce::Random` per-voice.

Anything that must live on the audio thread should be **pre-allocated, pre-sized, and pre-initialised in `prepareToPlay`**, and released in `releaseResources`.

---

## 3. Project Layout

Standard layout for a JUCE CMake plugin:

```
MyPlugin/
  CMakeLists.txt
  JUCE/                       # git submodule pinned to a known tag
  Source/
    PluginProcessor.h/.cpp
    PluginEditor.h/.cpp
    dsp/                      # pure DSP, no JUCE GUI includes
      Filter.h/.cpp
      Envelope.h/.cpp
    gui/                      # UI components, LookAndFeel
      MainPanel.h/.cpp
      Theme.h/.cpp
    params/
      ParameterIDs.h          # single source of truth for param IDs
      ParameterLayout.h/.cpp  # builds APVTS layout
  Resources/                  # binary assets (fonts, SVG, IR files)
  Tests/                      # juce::UnitTest suites
  .clang-format
  README.md
```

Keep DSP code **free of JUCE GUI headers** so it can be unit-tested and reused. `dsp/` should only include `juce_audio_basics`, `juce_dsp`, and `juce_core` where needed.

---

## 4. CMake

- Pin JUCE as a git submodule to a specific tag (e.g. `8.0.4`), never track `develop`.
- Use `juce_add_plugin` with an explicit `FORMATS` list (`VST3 Standalone AU` on macOS, `VST3 Standalone` on Windows/Linux). Only add `AAX` / `LV2` when actually needed.
- Set `COPY_PLUGIN_AFTER_BUILD TRUE` for local dev, off in CI.
- Give the plugin a stable `PLUGIN_CODE` and `PLUGIN_MANUFACTURER_CODE` and never change them post-release.
- On Windows, force static MSVC runtime to avoid missing-DLL issues on end-user machines:

  ```cmake
  # cmakelists: pin static msvc runtime on windows
  if (WIN32)
      set(CMAKE_MSVC_RUNTIME_LIBRARY
          "MultiThreaded$<$<CONFIG:Debug>:Debug>"
          CACHE INTERNAL "")
  endif()
  ```

- Prefer `target_compile_features(MyPlugin PRIVATE cxx_std_20)` (or 17 if you must). Do not rely on GNU extensions.
- Enable warnings as errors in CI: `/W4 /WX` on MSVC, `-Wall -Wextra -Wpedantic -Werror` on Clang/GCC.

---

## 5. Parameters: use APVTS, correctly

- Use a single `juce::AudioProcessorValueTreeState` owned by the processor.
- Build the layout with `AudioProcessorValueTreeState::ParameterLayout` and modern parameter classes: `juce::AudioParameterFloat`, `AudioParameterChoice`, `AudioParameterBool`, `AudioParameterInt`.
- Keep parameter IDs in a single header (`ParameterIDs.h`) as `constexpr` string literals. Never duplicate string IDs.
- Cache raw pointers once in `prepareToPlay` or in the processor constructor:

  ```cpp
  // processor: cache raw atomic pointer once, read every block
  gainParam = apvts.getRawParameterValue(ParamIDs::gain);
  // in processBlock:
  const float gain = gainParam->load();
  ```

- Use `SliderAttachment`, `ButtonAttachment`, `ComboBoxAttachment` in the editor. Do not hand-wire listeners for GUI controls.
- Save/restore with `apvts.copyState()` in `getStateInformation` and `apvts.replaceState()` in `setStateInformation`. Both use locks; that is fine because they run on the message thread.
- **Never** call GUI functions from `AudioProcessorValueTreeState::Listener::parameterChanged` directly. Host automation drives it from the audio thread. Marshal to the message thread via `juce::MessageManager::callAsync` or an `AsyncUpdater`.

---

## 6. DSP patterns

- Prefer `juce::dsp` modules (`ProcessorChain`, `IIR::Filter`, `Oversampling`, `Convolution`, `StateVariableTPTFilter`, `Gain`, `DryWetMixer`) over hand-rolled DSP unless you have a reason.
- Call `prepare(juce::dsp::ProcessSpec)` on every DSP object in `prepareToPlay`. Call `reset()` there too so state does not leak between sessions or when the transport jumps.
- Wrap parameter changes with `juce::SmoothedValue<float, ValueSmoothingTypes::Linear>` to kill zipper noise. Set the ramp length once (e.g. 20 ms) with `reset(sampleRate, rampSeconds)` in `prepareToPlay`.
- Denormals: put `juce::ScopedNoDenormals noDenormals;` at the very top of `processBlock`. Every plugin, every time.
- Handle **variable block sizes**. Never assume `numSamples` equals what you got in `prepareToPlay`. Some hosts call with 1 sample.
- Handle bus layouts explicitly in `isBusesLayoutSupported`. At minimum support mono-in/mono-out and stereo-in/stereo-out. Reject anything else.
- Clear unused output channels at the top of `processBlock`:

  ```cpp
  // processor: clear channels the host allocated but we won't write
  for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
      buffer.clear(i, 0, buffer.getNumSamples());
  ```

- If you oversample, wrap the whole chain in `juce::dsp::Oversampling` and keep the factor a parameter, not a compile-time constant.
- MIDI: iterate `MidiBuffer` with the range-based API and process events at their sample position, not the top of the block.

---

## 7. Threading

There are three threads to keep straight:

1. **Audio thread** (realtime). Read cached atomics, write to the buffer, return. Nothing else.
2. **Message thread** (GUI, host automation dispatch, timers). All `Component` calls must happen here.
3. **Background threads** (file loading, IR loading, analysis). Use `juce::Thread` or `juce::ThreadPool`.

Cross-thread communication:

- Audio -> GUI: `AbstractFifo` + a preallocated ring buffer, or `juce::AsyncUpdater`, or a lock-free single-producer / single-consumer queue.
- GUI -> audio: atomics (via APVTS) or a lock-free command queue.
- Background -> audio: swap-pointer pattern (build off-thread, atomic-swap in). Never mutate in place.

Do not use `std::atomic<T>` where `T` is not lock-free. Check with `std::atomic<T>::is_always_lock_free`.

---

## 8. GUI

- Keep it simple. Do all drawing in `paint`. Do not build a "framework within a framework" of LookAndFeels for a single-plugin codebase.
- Cache expensive objects (`juce::Path`, `juce::Image`, `juce::ColourGradient`) as members. Do not allocate them in `paint`.
- Use `setBufferedToImage(true)` for components that are expensive to draw and rarely change.
- Use `juce::Timer` (30 Hz is usually enough) to pull meter/analysis data from a `AbstractFifo`, not to poll parameters. Parameter -> UI goes through attachments.
- Support DAW-driven resize: implement `AudioProcessorEditor::resized()` cleanly with `juce::FlexBox` or `juce::Grid`, avoid absolute pixel positions.
- Restore editor size in `setStateInformation` if you offer resizing.
- Use OpenGL only if you actually need it. It has real costs on some hosts.

---

## 9. Testing

- Add `juce::UnitTest` suites under `Tests/` for every DSP class.
- Run **pluginval** (`--strictness-level 10`) in CI on every PR. It catches thread-safety and lifecycle bugs no unit test will find.
- Test in at least: Reaper, Ableton Live, FL Studio, Cubase, Logic (mac). Each host abuses plugins in different ways.
- Use `AudioPluginHost` (ships with JUCE) for quick local checks.
- On macOS, `auval -v aumu Manu Plug` for AU validation.

---

## 10. State & Presets

- `getStateInformation` / `setStateInformation` must be backward-compatible forever. Version your XML/ValueTree with a `pluginVersion` attribute so future code can migrate old sessions.
- Never store absolute file paths in state. Store relative paths or embed data.
- Presets: use `juce::ValueTree` and serialise with `writeToStream` / `readFromStream`. Ship factory presets as `BinaryData` via `juce_add_binary_data`.

---

## 11. Formats and gotchas

- **VST3**: parameter count and IDs are locked once shipped. Adding params later is fine; removing or renumbering breaks user sessions.
- **AU (macOS)**: bundle must live in `/Library/Audio/Plug-Ins/Components` or `~/Library/Audio/Plug-Ins/Components` to be recognised. Same for VST3 under `VST3`.
- **AAX**: requires Avid PACE signing to run outside dev mode. Do not attempt without the SDK license.
- **Standalone**: useful for debugging DSP without a host, but the standalone wrapper does not exercise host automation paths. Always also test as VST3.

---

## 12. Definition of done for a JUCE code change

Claude Code must satisfy all of these before declaring a task complete:

- [ ] context7 was queried for every JUCE symbol touched.
- [ ] `processBlock` allocates nothing, locks nothing, blocks nothing.
- [ ] `ScopedNoDenormals` is present in `processBlock`.
- [ ] All DSP objects have `prepare()` and `reset()` called in `prepareToPlay`.
- [ ] Parameters are wired through APVTS with cached raw pointers on the audio side and Attachments on the GUI side.
- [ ] Smoothing (`SmoothedValue`) is applied to any parameter that maps to gain, cutoff, or anything continuous.
- [ ] Bus layouts are validated in `isBusesLayoutSupported`.
- [ ] Unused output channels are cleared.
- [ ] State save/load round-trips cleanly and is version-tagged.
- [ ] Builds clean with `-Wall -Wextra -Wpedantic` (or `/W4` on MSVC) with no warnings.
- [ ] pluginval `--strictness-level 10` passes.

If any box is unchecked, the task is not done. Say so explicitly instead of claiming success.
