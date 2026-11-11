# GUI Architecture

The editor is `Source/PluginEditor.*`; panels, illustration and controls are in `Source/UI/` (Easy and Advanced panels, header, fretboard, guitar illustration, preset browser). The pedalboard has its own interface in `Source/UI/ReferencePedals/` (board root and canvas, pedal views and faces, browser, MIDI panel), hosted either in a drawer in the editor or in a pop-out window.

Read: [spec/gui-integration.md](https://github.com/djshellshoxxx/luthier/blob/master/spec/gui-integration.md) (where each control lives), [spec/ui-wiring.md](https://github.com/djshellshoxxx/luthier/blob/master/spec/ui-wiring.md) (attachments), [THEME_AS_BUILT.md](https://github.com/djshellshoxxx/luthier/blob/master/docs/THEME_AS_BUILT.md) (theme), [UI_BOARD_STYLES.md](https://github.com/djshellshoxxx/luthier/blob/master/spec/reference-pedalboard/specs/UI_BOARD_STYLES.md) (board interface).
