# Current State

## SC-UI-3 — Implemented (v0.2.0-beta.5)

File: `native-taskbar-media-controller.wh.cpp`  
Version: `0.2.0-beta.5`  
GitHub: https://github.com/StarlightDaemon/Native-Taskbar-Media-Controller  
Latest release tag: `v0.2.0-beta.5` (2026-05-23)

**Branch state:**
- `main` — fully pushed and tagged at `v0.2.0-beta.5`

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
- **SC-UI-2:** Adaptive text color — follows Windows light/dark app theme (`IsSystemLightTheme`); near-black in light mode, white in dark mode; applied to text, buttons, session chip, and progress bar; gated by `AdaptiveTextColor` setting
- **SC-M-2:** Double-tap widget raises source app (restore if minimized, minimize if open); `PKEY_AppUserModel_ID` property store matching + exe-name fallback; confirmed working with Spotify Store and Libby/Chrome
- **SC-KV-4:** Track progress bar — 3px bar at widget bottom; `Visibility::Collapsed` until `durationMs > 0`; gated by `ShowProgress` setting
- **Acrylic background:** `AcrylicBrush(HostBackdrop)` on widget root; falls back to semi-transparent dark if compositor rejects
- **SC-UI-3 — Grid layout refactor:** Replaced horizontal `StackPanel` with a 6-column `Grid` (Auto, Auto, `*`, Auto, Auto, Auto); text column fills remaining space via star sizing; removed hardcoded `MaxWidth(180)`
- **SC-UI-3 — Marquee scroll:** Long titles scroll smoothly when wider than clip container — `DispatcherTimer` at 16 ms drives a `TranslateTransform` inside a `Border(ClipToBounds)`; 2 s start-pause → 40 px/s left → 1 s end-pause → instant reset; fires only on overflow; gated by `MarqueeTitle` setting (default true); artist row unchanged (CharacterEllipsis)

**Cold-boot crash — RESOLVED (2026-05-22, beta.2.8):**
Explorer crashed 100% of the time on true cold boot because `Wh_ModInit` created 3 threads during Explorer's hazardous early-boot window. Fixed by reducing cold-start `Wh_ModInit` to a single poll thread; all other initialization deferred to `PollForTaskbarViewDll` after `Taskbar.View.dll` is confirmed loaded.

**Open loops:**
- OL-9: Phase 3 background theming (`BackgroundStyle` setting — None / Acrylic / Chameleon) — not yet started
- BootLog diagnostic calls intentionally retained through v1.0.0 — strip at release time

**Phase 2 confirmed scope:** SC-CH-1 ✓, SC-UI-2 ✓, SC-M-2 ✓, SC-KV-4 ✓  
**SC-UI-3 confirmed scope:** Grid layout refactor ✓, marquee scroll ✓  
**Phase 3 confirmed scope:** `BackgroundStyle` setting — None / Acrylic / Chameleon (OL-9)

**Next:** Phase 3 — `BackgroundStyle` setting (OL-9). Gate: confirm `AcrylicBrush` composites correctly in Explorer's XAML tree before shipping Acrylic option.
