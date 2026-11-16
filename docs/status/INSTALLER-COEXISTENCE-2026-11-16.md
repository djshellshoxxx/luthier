# Installer edition coexistence — 2026-11-16

Free and Pro share factory content. The old macOS uninstaller deleted the whole
shared support directory even when another edition remained, and the Windows
installer treated its factory files as exclusively owned by each edition.

The macOS reproduction adds a factory-file assertion to the existing two-edition
fixture. It fails before repair. The uninstaller now checks all four other-edition
formats, retains shared Resources while any remain, and removes factory Resources
only after the last edition is removed. Independently managed machine policy is
preserved. Scratch-root tests no longer touch real package receipts or the audio
registrar. All 28 fixture checks pass, including both removal orders, plugin-only
coexistence and final cleanup. These check script behavior, not Installer.app.

Windows factory files now use Inno Setup `sharedfile uninsnosharedfileprompt`.
Administrator-supplied machine policy uses `uninsneveruninstall`: removing either
edition, including the final edition, must not delete independently managed
policy. The fixture supplies policy through the first edition, reinstalls it,
and verifies its exact contents after both removals.
Its standard reference count includes reinstalls and is decremented by the same
number on uninstall, retaining a file until all owners are removed. Reference:
https://jrsoftware.org/inno-setup/documentation/topic_filessection.htm#sharedfile

`scripts/test_install_windows.ps1` compiles the actual installer definition with
synthetic payloads and exercises both edition orders plus reinstall and cleanup.
It is restricted to clean, disposable GitHub-hosted Windows runners and does not
launch the synthetic binaries. The release pipeline requires this check before
native compilation/package qualification and retains installer logs. The initial Windows factory-content fixture passed both removal orders in
release run 43915893590, job 133470310776. Execution of the extended policy
retention fixture remains pending at this commit; local review alone does not
establish its native success. Older installed uninstallers are not retroactively
changed by this repair.
