All rows are OWNED by the release helper (licensing deferred to the editions split); no Phase-2 fixes. Pre-existing code the owner must integrate with:

- NOTE `Source/Updates/Telemetry.h:301 class License` (+ `Telemetry.cpp`) — placeholder: `State { unlicensed, activated, grace, expired }`, `activate`, `revalidate`, `deactivate`, `getOfflineChallenge`; accepts any successful HTTP response / any 16-char offline response. Replace per §11 and move to a Pro-only file.
- NOTE Existing tests `Telemetry::licenceActivationAndGrace` (`Source/Tests/TelemetryTests.cpp:550`) exercise the placeholder semantics and will need rewriting along with it.
- NOTE UI points to keep: header grace countdown `Source/PluginEditor.cpp:834-835` (listed at :801), HELP tab licence line `Source/UI/HelpTab.cpp:164-173`, help text `Source/UI/HelpContent.cpp:444`. There is no Options -> Licence page (the Options page list in `GuiReachabilityTests.cpp:~506` has none); the spec and updates-telemetry.md 5 assume one.
- NOTE `Transport` abstraction and privacy/network log in `Source/Updates/` — licensing calls must go through it.
