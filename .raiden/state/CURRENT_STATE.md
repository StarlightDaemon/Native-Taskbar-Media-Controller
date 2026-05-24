# Current State

## v1.0.1 — In progress (2026-05-24)

File: `native-taskbar-media-controller.wh.cpp`  
Version: `1.0.1`  
GitHub: https://github.com/StarlightDaemon/Native-Taskbar-Media-Controller  
Latest release tag: `v1.0.0` (2026-05-24)

**Branch state:**
- `main` — fully pushed and tagged at `v1.0.0`

**What works:**
- Native XAML injection into `Grid#RootGrid` under `Taskbar.TaskbarFrame` (no overlay window)
- GSMTC multi-session array (`g_MediaStates[10]`) with per-session `MediaPropertiesChanged` and `PlaybackInfoChanged` subscriptions
- Title/artist `TextBlock` display, play/pause toggle, next-track button
- Session cycle chip (session count shown when >1, tap cycles `g_ActiveSessionIndex`)
- Tray-width-aware `Margin.Right` via `UpdateWidgetMargin()` + `SizeChanged` subscription on `SystemTrayFrameGrid`
- Fullscreen hiding via `SHQueryUserNotificationState` poll thread (1s interval); per-monitor aware
- Clean unload: widget removed from XAML tree, all event tokens revoked, hook counter drain
- **Cover art:** square `Image` element, `BitmapImage` loaded via `co_await OpenReadAsync` + `SetSourceAsync`, marshalled back to UI dispatcher; null-thumbnail collapses gracefully
- **Libby audiobook support:** AlbumTitle/AlbumArtist fallback, playback rate suffix (` · 1.5×`), `«`/`»` skip buttons gated on `IsPreviousEnabled`/`IsNextEnabled` (previous/next track or chapter)
- **Hardening:** `g_GsmtcStartEvent` converted to `std::atomic<HANDLE>` (TOCTOU fix); uninit drain raised to 5 s with timeout warning
- **SC-CH-1:** `IsTaskbarEffectivelyVisible` — widget hides when taskbar auto-hides to ≤30px strip
- **SC-UI-2:** Adaptive text color — follows Windows light/dark app theme (`IsSystemLightTheme`); near-black in light mode, white in dark mode; gated by `AdaptiveTextColor` setting
- **SC-M-2:** Double-tap widget raises source app (restore if minimized, minimize if open); `PKEY_AppUserModel_ID` property store matching + exe-name fallback
- **SC-KV-4:** Track progress bar — 3px bar at widget bottom; `Visibility::Collapsed` until `durationMs > 0`; gated by `ShowProgress` setting
- **SC-UI-3 — Grid layout refactor:** 6-column `Grid` (Auto, Auto, `*`, Auto, Auto, Auto); text column fills remaining space via star sizing
- **SC-UI-3 — Marquee scroll:** `LayoutUpdated` + `Storyboard/DoubleAnimation`; fires only when title overflows clip container; gated by `MarqueeTitle` setting
- **OL-9 — BackgroundStyle:** None / Acrylic / Chameleon; Chameleon derives `LinearGradientBrush` from album art via 64-bucket RGB histogram; all modes live-applied in `ApplyStateToWidget()`
- **Log cleanup:** Verbose trace logs stripped; only error and warning paths remain

**Cold-boot crash — RESOLVED (2026-05-22, beta.2.8):**
`Wh_ModInit` reduced to a single poll thread; all other initialization deferred to `PollForTaskbarViewDll` after `Taskbar.View.dll` is confirmed loaded.

**Phase 2 confirmed scope:** SC-CH-1 ✓, SC-UI-2 ✓, SC-M-2 ✓, SC-KV-4 ✓  
**SC-UI-3 confirmed scope:** Grid layout refactor ✓, marquee scroll ✓  
**Phase 3 confirmed scope:** `BackgroundStyle` — None / Acrylic / Chameleon ✓  
**Release gate:** Log cleanup ✓ — v1.0.0 tagged and pushed

**v1.0.1 patch (2026-05-24) — SMTC compatibility audit fixes:**
- **Docs:** Corrected in-mod readme compatibility notes for Firefox and Chromium browsers (both incorrectly claimed timeline data was available)
- **Defect:** `ExtractExeHint` now handles Store AUMIDs with a generic `!App` AppId; Edge Store AUMID (`Microsoft.MicrosoftEdge.Stable_8wekyb3d8bbwe!App`) now correctly resolves to `"msedge"` and classifies as `Browser`
- **Defect:** `ClassifySessionSource` gains a substring fallback for unknown Store AUMIDs where `ExtractExeHint` returns a full package family name
- **Defect:** Audiobook detection adds a secondary heuristic — `"Chapter "` keyword in title + duration > 15 minutes — to catch short Libby/Audiobookshelf chapters that would previously fall through as NativeApp music

**Post-v1.0.1 candidates (ordered by value):**
1. ~~SC-SP-1 — Interactive seek bar~~ **NOT WANTED — will not be implemented.** Explicitly out of scope; do not revisit.
2. SC-UI-1 — Blurred album art background (rendering pipeline review needed)
3. SC-HT-1 — LRC lyrics overlay (significant scope increase)
4. SC-GR-1 — FFT audio visualizer (process compatibility audit required)
