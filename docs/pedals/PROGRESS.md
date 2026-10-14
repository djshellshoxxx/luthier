# Pedal models feature: progress log

Branch: claude/vigilant-babbage-2dwdec. Updated at each milestone; the 6-hourly check reads this file.

## Decisions
- D1 Trademark policy (spec/roadmap/v2/cables-and-brands.md rule 1, Tools/trademark_scan.py, TrademarkTests): no brand or product name in code, data or UI. Pedals are modelled on the real circuits and faces reproduce enclosure colour, knob layout and finish, but ship under original generic names and without logos. Real names appear only in docs/research.
- D2 Accuracy gate: a pedal ships only if its research note has Verdict MODEL (component values corroborated by >=2 sources). Otherwise it is skipped.
- D3 Waiting for the ChatGPT pedal spec; it will be reconciled with spec/design/v2/pedalboard before building.

## Milestones
- [x] M0 Repo survey, baseline build started, research workers launched (4 categories)
- [ ] M1 Research complete, spec reconciled, architecture doc
- [ ] M2 Core: model registry, pedal DSP modules, faces, UI add/remove/swap
- [ ] M3 Audit + extensive tests + QA, fixes
- [ ] M4 Windows Pro build green
