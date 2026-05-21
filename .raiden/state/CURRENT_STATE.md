# Current State

## Phase 1 — Complete and Released

File: `native-taskbar-media-controller.wh.cpp`  
Version: `0.1.0-beta.1`  
GitHub: https://github.com/StarlightDaemon/Native-Taskbar-Media-Controller  
Release: https://github.com/StarlightDaemon/Native-Taskbar-Media-Controller/releases/tag/v0.1.0-beta.1

**Branch state:**
- `main` — Phase 1 complete, README present, `v0.1.0-beta.1` tagged and pushed
- `feature/rename-to-native-controller` — fully merged into `main`; pushed to remote

**What works:**
- Native XAML injection into `Grid#RootGrid` under `Taskbar.TaskbarFrame` (no overlay window)
- GSMTC multi-session array (`g_MediaStates[10]`) with per-session `MediaPropertiesChanged` and `PlaybackInfoChanged` subscriptions
- Title/artist `TextBlock` display, play/pause toggle, next-track button
- Session cycle chip (session count shown when >1, tap cycles `g_ActiveSessionIndex`)
- Tray-width-aware `Margin.Right` via `UpdateWidgetMargin()` + `SizeChanged` subscription on `SystemTrayFrameGrid`
- Fullscreen hiding via `SHQueryUserNotificationState` poll thread (1s interval)
- Clean unload: widget removed from XAML tree, all event tokens revoked, hook counter drain

**Next:** Phase 2 feature selection — see OPEN_LOOPS.md and GOALS.md.
