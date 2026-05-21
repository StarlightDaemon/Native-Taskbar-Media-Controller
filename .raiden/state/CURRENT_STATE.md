# Current State

## Phase 1 — Complete

File: `native-taskbar-media-controller.wh.cpp`

The Phase 1 skeleton is built and committed on branch `feature/rename-to-native-controller`.

**What works:**
- Native XAML injection into `Grid#RootGrid` under `Taskbar.TaskbarFrame` (no overlay window)
- GSMTC multi-session array (`g_MediaStates[10]`) with per-session `MediaPropertiesChanged` and `PlaybackInfoChanged` subscriptions
- Title/artist `TextBlock` display, play/pause toggle, next-track button
- Session cycle chip (session count shown when >1, tap cycles `g_ActiveSessionIndex`)
- Tray-width-aware `Margin.Right` via `UpdateWidgetMargin()` + `SizeChanged` subscription on `SystemTrayFrameGrid`
- Fullscreen hiding via `SHQueryUserNotificationState` poll thread (1s interval)
- Clean unload: widget removed from XAML tree, all event tokens revoked, hook counter drain

**Known open items (see OPEN_LOOPS.md):**
- Phase 2 feature selection pending operator decision
- OffsetX default in settings (8px) vs struct default (200px) — mismatch; struct wins at runtime because `LoadSettings` clamps negative values but not the struct default. Needs reconciliation.
