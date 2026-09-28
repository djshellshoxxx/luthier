# Easy amp layout coverage

| Requirement | Implementation | Verification |
|---|---|---|
| The six Easy-mode amp knobs have usable face space at 1200×720. | `Source/UI/EasyPanel.cpp` gives the amp card 39% of the rig height; compact neighboring cards are slightly shorter. | `EasyLayout.ampKnobsHaveRoomAtCompactWindowSize` checks two rows and at least 40×40 px per knob. Build and run pending coordinator Linux gate. |
