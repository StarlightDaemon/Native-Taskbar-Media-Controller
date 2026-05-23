# Current State

## Phase 2 — Implemented (v0.2.0-beta.2)

File: `native-taskbar-media-controller.wh.cpp`  
Version: `0.2.0-beta.4`  
GitHub: https://github.com/StarlightDaemon/Native-Taskbar-Media-Controller  
Latest release tag: `v0.1.0-beta.2.8` (Phase 2 pending operator test + push)

**Branch state:**
- `main` — Phase 2 implemented locally, not yet tagged or pushed

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
- **SC-CH-1:** `IsTaskbarEffectivelyVisible` — widget hides when taskbar auto-hides to ≤30px strip
- **SC-UI-2:** Adaptive text color — title/artist foreground adapts to album art luminance (BT.601 luma < 135 → dark cover → white text; else near-black text); gated by `AdaptiveTextColor` setting
- **SC-M-2:** `BringSourceAppToFront` — double-tap widget raises source app window via `PKEY_AppUserModel_ID` property store matching + exe-name fallback; `-lpropsys` added to compiler options
- **SC-KV-4:** Track progress bar — 3px bar at widget bottom, `Visibility::Collapsed` until `durationMs > 0`; position populated from `GetTimelineProperties()`; gated by `ShowProgress` setting

**Cold-boot crash — RESOLVED (2026-05-22, beta.2.8):**
Explorer crashed 100% of the time on true cold boot because `Wh_ModInit` created 3 threads and called `init_apartment(multi_threaded)` during Explorer's hazardous early-boot window. Fixed by reducing cold-start `Wh_ModInit` to a single poll thread; all other initialization deferred to `PollForTaskbarViewDll` after `Taskbar.View.dll` is confirmed loaded. Confirmed resolved by operator.

**Open loops:**
- OL-2: Phase 2 — confirmed complete (all four features operator-verified)
- OL-9: Phase 3 background theming defined (Acrylic + Chameleon as `BackgroundStyle` setting) — not yet started
- BootLog diagnostic calls intentionally retained through v1.0.0 — strip at release time

**Phase 2 confirmed scope:** SC-CH-1 ✓, SC-UI-2 ✓, SC-M-2 ✓, SC-KV-4 ✓  
**Phase 3 confirmed scope:** `BackgroundStyle` setting — None / Acrylic / Chameleon (SC-UI-1 concept + SC-HT-2 concept, XAML-native implementations)

**SC-M-2 status:** Confirmed working as of live test 2026-05-23 (v0.2.0-beta.3). Two root causes fixed:
1. `AttachThreadInput` operands were wrong — was attaching `fgTid→tgtTid`; fixed to `ourTid→fgTid` so the XAML dispatcher thread (the actual caller) acquires the foreground lock. Added `AllowSetForegroundWindow(ASFW_ANY)` as belt-and-suspenders.
2. `ExtractExeHint` was broken for Store AUMIDs — took pre-`!` package name (`spotifyab.spotifymusic_zpdnekdrzrea0`) instead of post-`!` AppId (`spotify`); fixed to detect Store AUMIDs (pre-bang doesn't end in `.exe`) and use the post-bang AppId as the exe hint.

**SC-M-2 toggle (v0.2.0-beta.4):** Double-tap now toggles — minimized window restores+raises; open window minimizes. Uses `IsIconic` heuristic, matching Windows taskbar button behavior. Not yet operator-verified.

**Next:** Operator live test of toggle behavior, then tag + push `v0.2.0-beta.4`. Phase 3 — `BackgroundStyle` setting (OL-9) follows.
