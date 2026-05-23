# Current State

## Phase 1 — Complete and Released (v0.1.0-beta.2) + Cold-Boot Fix (v0.1.0-beta.2.8)

File: `native-taskbar-media-controller.wh.cpp`  
Version: `0.1.0-beta.2.8`  
GitHub: https://github.com/StarlightDaemon/Native-Taskbar-Media-Controller  
Latest release tag: `v0.1.0-beta.2.8`

**Branch state:**
- `main` — up to date, `v0.1.0-beta.2.8` tagged and pushed

**What works:**
- Native XAML injection into `Grid#RootGrid` under `Taskbar.TaskbarFrame` (no overlay window)
- GSMTC multi-session array (`g_MediaStates[10]`) with per-session `MediaPropertiesChanged` and `PlaybackInfoChanged` subscriptions
- Title/artist `TextBlock` display, play/pause toggle, next-track button
- Session cycle chip (session count shown when >1, tap cycles `g_ActiveSessionIndex`)
- Tray-width-aware `Margin.Right` via `UpdateWidgetMargin()` + `SizeChanged` subscription on `SystemTrayFrameGrid`
- Fullscreen hiding via `SHQueryUserNotificationState` poll thread (1s interval); per-monitor aware
- Clean unload: widget removed from XAML tree, all event tokens revoked, hook counter drain
- **Cover art:** square `Image` element, `BitmapImage` loaded via `co_await OpenReadAsync` + `SetSourceAsync`, marshalled back to UI dispatcher; null-thumbnail collapses gracefully
- **Libby audiobook support:** AlbumTitle/AlbumArtist fallback, playback rate suffix (` · 1.5×`), `«`/`»` skip buttons gated on `IsSkipForward/BackwardEnabled`
- **Hardening:** `g_GsmtcStartEvent` converted to `std::atomic<HANDLE>` (TOCTOU fix); uninit drain raised to 5 s with timeout warning

**Cold-boot crash — RESOLVED (2026-05-22, beta.2.8):**
Explorer crashed 100% of the time on true cold boot because `Wh_ModInit` created 3 threads and called `init_apartment(multi_threaded)` during Explorer's hazardous early-boot window. Fixed by reducing cold-start `Wh_ModInit` to a single poll thread; all other initialization deferred to `PollForTaskbarViewDll` after `Taskbar.View.dll` is confirmed loaded. Confirmed resolved by operator.

**Open loops:**
- OL-2: Phase 2 feature selection (operator action required — see OPEN_LOOPS.md)
- BootLog diagnostic calls intentionally retained through v1.0.0 — cold-boot fix is new and the log catches any regression immediately; strip at release time

**Next:** Phase 2 feature selection — see OPEN_LOOPS.md and GOALS.md.
