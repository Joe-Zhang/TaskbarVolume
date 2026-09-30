# Verification

Current version verified locally on Windows x64, 2026-09-30.

- Mouse location uses the original GetCursorPos method.
- No DPI-awareness switching remains in production code.
- MSVC production build: /O2 /W4 /WX, no warnings.
- Integration suite: all 42 checks passed after restoring GetCursorPos.
- Logical taskbar coordinates verified at the current 150% display scaling.
- Partial wheel accumulation, negative steps, deferred audio handling, volume
  boundaries, mute/unmute, device notification handling, tray recovery,
  overlay timers, Settings close/reopen, and resource cleanup passed.
- 300 overlay redraws kept the GDI object count stable.
- Original volume and mute state restored after tests.
- Root TaskbarVolume.exe replaced and started by absolute path; process path verified.

The integration suite tests the wheel-message queue directly. It does not
inject physical wheel input. Physical unplug/replug, actual Explorer restart,
multiple monitors, startup registry writes and long-term monitoring are not covered.

Earlier diagnostics confirmed the regression came from mixing physical hook
coordinates with DPI-unaware hit-test coordinates. A temporary DPI-context fix
was tested, then removed in favor of the original simpler GetCursorPos method.
The previous full executable startup/single-instance/exit tests and five-second
idle CPU sample passed before this final mouse-coordinate simplification.
