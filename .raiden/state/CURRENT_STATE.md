# Current State

Migration remediation: see OL-11.

---

## v1.5.0 — Released (2026-06-24)

File: `native-taskbar-media-controller.wh.cpp`
Version: `1.5.0`
GitHub: https://github.com/StarlightDaemon/Native-Taskbar-Media-Controller
Release tag: `v1.5.0` (2026-06-24, commit `59e2a37`)

**Branch state:**
- `main` — v1.5.0 tagged and pushed at `59e2a37`

**New features (97096a7 + ffd5ba7):**

- **Widget position setting** — `WidgetPosition` dropdown (Right / Left / Center) added as the first setting in the Windhawk UI. `BuildWidget()` picks `HorizontalAlignment` from the mode at construction; `Wh_ModSettingsChanged()` applies it live. `UpdateWidgetMargin()` branches per mode: Right tracks tray width + updates `g_FlyoutMarginDIPs`; Left/Center apply a fixed `Margin.Left = offsetX`. Flyout `WM_FLYOUT_SHOW` computes screen-x per mode using the taskbar RECT. Tray `SizeChanged` subscription is gated to Right mode only. `OffsetX` meaning adapts: gap from tray (Right), gap from left edge (Left), nudge from center (Center, positive shifts right). Settings block reordered: `WidgetPosition` + `OffsetX` promoted to top.
- **Dynamic flyout title font sizing** — flyout title steps down from 13pt to 9pt (1pt at a time) until the rendered `GetTextExtentPoint32W` width fits the available area; `DT_END_ELLIPSIS` is the final fallback if the text is still too wide at 9pt. Replaces the previous fixed 13pt `CreateFontW` call.

**Audit-cycle remediation (2026-06-24):**

- **F2a — `g_TrayResizeToken` not revoked on uninit** (`aaf3e4a`): `RemoveWidget` now revokes `g_TrayResizeToken` before the widget is detached, closing the unrevoked-subscription teardown gap flagged in the 2026-06-24 audit.
- **F2b — Detached `InitialScan` and `PollForDll` threads never joined in `Wh_ModUninit`** (`59e2a37`): both threads are now joined in `Wh_ModUninit`, preventing use-after-free on hot unload.

---

## v1.4.8 — Released (2026-05-25)

File: `native-taskbar-media-controller.wh.cpp`  
Version: `1.4.8`  
GitHub: https://github.com/StarlightDaemon/Native-Taskbar-Media-Controller  
Latest release tag: `v1.4.8` (2026-05-25) — superseded by v1.4.9

**Branch state:**
- `main` — v1.4.8 released; v1.4.9 followed

**What works:**
- Native XAML injection into `Grid#RootGrid` under `Taskbar.TaskbarFrame` (no overlay window)
- GSMTC multi-session array (`g_MediaStates[10]`) with per-session `MediaPropertiesChanged` and `PlaybackInfoChanged` subscriptions
- Title/artist `TextBlock` display, play/pause toggle, next-track button
- Session cycle chip (session count shown when >1, tap cycles `g_ActiveSessionIndex`)
- Tray-width-aware `Margin.Right` via `UpdateWidgetMargin()` + `SizeChanged` subscription on `SystemTrayFrameGrid`
- Fullscreen hiding via `SHQueryUserNotificationState` poll thread (1s interval); per-monitor aware
- Clean unload: widget removed from XAML tree, all event tokens revoked, hook counter drain
- **Cover art:** square `Image` element, `BitmapImage` loaded via `co_await OpenReadAsync` + `SetSourceAsync`, marshalled back to UI dispatcher; null-thumbnail collapses gracefully
- **Libby audiobook support:** AlbumTitle/AlbumArtist fallback, playback rate suffix (` · 1.5×`), skip buttons gated on `IsPreviousEnabled`/`IsNextEnabled`
- **Hardening:** `g_GsmtcStartEvent` converted to `std::atomic<HANDLE>` (TOCTOU fix); uninit drain raised to 5 s with timeout warning
- **SC-CH-1:** `IsTaskbarEffectivelyVisible` — widget hides when taskbar auto-hides to ≤30px strip
- **SC-UI-2:** Adaptive text color — follows Windows light/dark app theme; gated by `AdaptiveTextColor` setting
- **SC-M-2:** Double-tap widget raises source app; `PKEY_AppUserModel_ID` + exe-name fallback
- **SC-KV-4:** Track progress bar — 3px bar at widget bottom; gated by `ShowProgress` setting
- **SC-UI-3 — Grid layout:** 6-column `Grid`; text column fills remaining space via star sizing

**v1.0.1 patch (2026-05-24) — SMTC compatibility audit fixes:**
- **Docs:** Corrected in-mod readme compatibility notes for Firefox and Chromium browsers
- **Defect:** `ExtractExeHint` handles Store AUMIDs with generic `!App` AppId; Edge now correctly classifies as `Browser`
- **Defect:** `ClassifySessionSource` gains substring fallback for unknown Store AUMIDs
- **Defect:** Audiobook detection adds `"Chapter "` keyword + duration > 15 min secondary heuristic

**v1.1.0 — Marquee scroll + UX polish (2026-05-24):**
- **Marquee scroll:** Seamless two-copy ticker — `Canvas` clip container wrapping a horizontal `StackPanel` (`kTitleScrollerName`) with title1 + 48px gap + title2; `DoubleAnimationUsingKeyFrames` on the scroller's `TranslateTransform.X`; 2s start pause → 50 px/s constant speed → invisible loop reset when canvas is blank; gated by `MarqueeScroll` setting (default true)
- **Scroll only resets on track change:** `ApplyStateToWidget` compares new title text to current before stopping animation — play/pause and other state updates leave the scroll running undisturbed
- **Play/pause button fixed width:** `Width(30.0)` prevents layout shift when toggling between ▶ and ⏸; both use `︎` text variation selector to force monochrome rendering
- **Skip buttons always visible:** Skip back/forward always shown; dimmed to `Opacity(0.35)` when source doesn't support them (browsers, etc.) instead of collapsing — consistent « ▶ » layout regardless of media source
- **Widget fills full taskbar height:** `VerticalAlignment::Stretch`, explicit `Height` removed — background fills edge-to-edge vertically

**v1.2.0 — Polish pass (2026-05-24):**
- **SC-GR-2 — Text crossfade:** 0.15 s opacity fade-out → text swap → fade-in on track change; play/pause and state-only updates bypass animation entirely; `g_TextFadeStoryboard` global; weak-ref captures in `Completed` lambda
- **SC-SP-4 — Widget fade in/out:** `SetWidgetVisible(UIElement, bool)` helper; 0.2 s `DoubleAnimation` on `Opacity`; fade-in sets `Opacity(0)` + `Visibility::Visible` before animating to prevent flash; fade-out defers `Visibility::Collapsed` to `Completed` callback; `g_WidgetFadeStoryboard` global
- **SC-HT-4 — Smooth progress interpolation:** `DispatcherTimer` at 500 ms (`g_ProgressTimer`); each tick reads session state under `g_MediaMutex`, advances display position by `GetTickCount64()` elapsed time, updates fill width + timestamp text; stops itself when paused; cleaned up in `Wh_ModUninit`

**v1.3.0 — Middle-click close + stability hardening (2026-05-24):**
- **SC-M-3 — Middle-click to close:** `PointerPressed` + `PointerUpdateKind::MiddleButtonPressed` → `TryCloseAsync()` on active session
- **Structural fixes:** `goto` replaced with `handledByFade` flag; `FormatMs` deduplication; `FindWindowW` moved outside mutex; `ComputeDominantColors` relocated
- **Threading fixes:** `DetachSessionLocked` refactored — COM revocations deferred out of `g_MediaMutex`; all XAML stops consolidated into `RemoveWidget`'s `RunAsync` lambda (fixes STA threading contract violation)

**v1.4.4 — SC-FLY-1 hover flyout panel (2026-05-25):**
- **SC-FLY-1 — Hover flyout:** Win32 `WS_POPUP` HWND on dedicated thread; shows on `PointerEntered`, hides 300ms after `PointerExited`; displays full-width album art square + title + artist; GDI paint with `HALFTONE` StretchBlt; dark/light mode via `DwmSetWindowAttribute(DWMWA_USE_IMMERSIVE_DARK_MODE)` + uxtheme ordinal 132; `WM_SETTINGCHANGE` listener for theme transitions; album art decoded to HBITMAP via `BitmapDecoder`+`GetPixelDataAsync`+`CreateDIBSection`; cross-thread via `PostMessageW` to dedicated flyout thread; `WS_EX_LAYERED` with configurable transparency; `FlyoutTransparent` setting

**v1.4.5–v1.4.7 — Flyout polish (2026-05-25):**
- Flyout sizing matches widget width; right-aligned to widget's right edge
- System theme colors — dark: neutral gray `RGB(28,28,28)`; light: `GetSysColor(COLOR_3DFACE)`
- Flyout font sizes reduced (13pt title, 11pt artist); `DT_END_ELLIPSIS` for overflow
- `FlyoutTransparent` setting: 92% opaque when enabled, solid when disabled

**v1.4.8 — Audit hardening (2026-05-25):**
- **Atomic flyout HWND:** `g_FlyoutHwnd` changed from plain `HWND` to `std::atomic<HWND>` — eliminates theoretical data race between UI thread reads and flyout thread writes
- **Flyout class cleanup:** `UnregisterClassW` called after flyout message loop exits — prevents stale class registration on hot reload
- **RemoveWidget threading fix:** `StopMarquee()` and `g_TitleSizeChangedToken` revocation moved entirely inside `RunAsync` lambda — all XAML cleanup now runs on the dispatcher thread, respecting the STA threading contract
- **`check_xaml.cpp` gitignored:** diagnostic probe excluded from version control
- **Documentation refresh:** CURRENT_STATE, README, GOALS updated to reflect v1.4.x changes and removed features

**Features removed in v1.4.x:**
- ~~BackgroundStyle setting (None / Acrylic / Chameleon)~~ — simplified to Acrylic-only; Chameleon and Blurred Art modes removed. The 64-bucket `ComputeDominantColors` helper, `g_ChameleonLightBg` atomic, and all `BackgroundStyle` enum/settings code deleted.

**Post-v1.4.8 candidates (ordered by value):**
1. ~~SC-SP-1 — Interactive seek bar~~ **NOT WANTED — will not be implemented.** Explicitly out of scope; do not revisit.
2. ~~SC-0X-1 — Display-only mode~~ **NOT WANTED — will not be implemented.**
3. SC-HT-1 — LRC lyrics overlay (significant scope increase; natural fit as flyout content tier 2)
4. SC-GR-1 — FFT audio visualizer (process compatibility audit required)

**v1.5 / post-release maybe:**
- **Chrome Extension companion** — A Chrome extension + Native Messaging host that relays richer Media Session state (chapter metadata, `setPositionState` data Libby doesn't push to SMTC) to the mod via named pipe or shared memory. Motivation: Libby is a Chrome PWA; its SMTC ceiling is whatever it publishes via `navigator.mediaSession`, and it doesn't call `setPositionState()`. A companion extension is the only clean path past that ceiling. Scope: extension + native host exe + IPC layer in mod — three moving parts, non-trivial install story. Revisit only after core mod is stable at v1.x.
