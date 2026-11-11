# Uncompiled drafts for the Advanced-column work (parked 2026-11-11)

These files were written but **never compiled or run**. They are kept here, outside `Source/`,
so the build ignores them. They belong to the tasks on this branch: one shared mouse-wheel policy
for controls inside scrolling columns, visible scrollbars, scroll indicators, and collapsible
sections.

| File | Intended destination | What it is |
|---|---|---|
| `WheelPolicy.h`, `WheelPolicy.cpp` | `Source/UI/` | shared wheel policy: a wheel event over any control inside a scrolling column scrolls the column |
| `CollapsibleSectionHeader.h`, `.cpp` | `Source/UI/` | section header that collapses and expands a titled section (click, Enter, Space) |
| `column_impl.cpp` | into `Source/UI/AdvancedPanel.cpp` | new column layout using the header and policy |
| `ColumnCollapseTests.cpp` | `Source/Tests/` | tests for collapse, persistence and reachability |
| `ScrollWheelTests.append.cpp` | append to `Source/Tests/ScrollWheelTests.cpp` | tests for the wheel policy (the committed RED tests on this branch cover part of this) |

Findings to keep in mind when finishing:

- No combo box turns off its own wheel handling today, so combo boxes still take the wheel.
- A workspace pad's wheel is how the reachability test reaches its angle parameter; leave that pad alone.
- Overflow arrows already exist in the columns, but they are not accessible buttons.
- Still to design: viewport and scrollbar changes, saving collapsed sections with the editor UI
  state (must load old states with everything expanded), and the microphone handle's wheel.
