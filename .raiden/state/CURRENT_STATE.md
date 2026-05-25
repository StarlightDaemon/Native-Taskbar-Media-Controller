# Current State

## v1.2.0 — Uncommitted (2026-05-24)

File: `native-taskbar-media-controller.wh.cpp`  
Version: `1.1.0`  
GitHub: https://github.com/StarlightDaemon/Native-Taskbar-Media-Controller  
Latest release tag: `v1.1.0` (2026-05-24)

**Branch state:**
- `main` — v1.2.0 uncommitted; v1.1.0 is latest pushed tag

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
- **OL-9 — BackgroundStyle:** None / Acrylic / Chameleon; Chameleon derives `LinearGradientBrush` from album art

**v1.1.0 — Marquee scroll + UX polish (2026-05-24):**
- **Marquee scroll:** Seamless two-copy ticker — `Canvas` clip container wrapping a horizontal `StackPanel` (`kTitleScrollerName`) with title1 + 48px gap + title2; `DoubleAnimationUsingKeyFrames` on the scroller's `TranslateTransform.X`; 2s start pause → 50 px/s constant speed → invisible loop reset when canvas is blank; gated by `MarqueeScroll` setting (default true)
- **Scroll only resets on track change:** `ApplyStateToWidget` compares new title text to current before stopping animation — play/pause and other state updates leave the scroll running undisturbed
- **Play/pause button fixed width:** `Width(30.0)` prevents layout shift when toggling between ▶ and ⏸; both use `︎` text variation selector to force monochrome rendering
- **Skip buttons always visible:** Skip back/forward always shown; dimmed to `Opacity(0.35)` when source doesn't support them (browsers, etc.) instead of collapsing — consistent « ▶ » layout regardless of media source
- **Widget fills full taskbar height:** `VerticalAlignment::Stretch`, explicit `Height` removed — background fills edge-to-edge vertically

**v1.0.1 patch (2026-05-24) — SMTC compatibility audit fixes:**
- **Docs:** Corrected in-mod readme compatibility notes for Firefox and Chromium browsers
- **Defect:** `ExtractExeHint` handles Store AUMIDs with generic `!App` AppId; Edge now correctly classifies as `Browser`
- **Defect:** `ClassifySessionSource` gains substring fallback for unknown Store AUMIDs
- **Defect:** Audiobook detection adds `"Chapter "` keyword + duration > 15 min secondary heuristic

**v1.2.0 — Polish pass (2026-05-24):**
- **SC-GR-2 — Text crossfade:** 0.15 s opacity fade-out → text swap → fade-in on track change; play/pause and state-only updates bypass animation entirely; `g_TextFadeStoryboard` global; weak-ref captures in `Completed` lambda
- **SC-SP-4 — Widget fade in/out:** `SetWidgetVisible(UIElement, bool)` helper; 0.2 s `DoubleAnimation` on `Opacity`; fade-in sets `Opacity(0)` + `Visibility::Visible` before animating to prevent flash; fade-out defers `Visibility::Collapsed` to `Completed` callback; `g_WidgetFadeStoryboard` global
- **SC-HT-4 — Smooth progress interpolation:** `DispatcherTimer` at 500 ms (`g_ProgressTimer`); each tick reads session state under `g_MediaMutex`, advances display position by `GetTickCount64()` elapsed time, updates fill width + timestamp text; stops itself when paused; cleaned up in `Wh_ModUninit`

**Post-v1.2.0 candidates (ordered by value):**
1. ~~SC-SP-1 — Interactive seek bar~~ **NOT WANTED — will not be implemented.** Explicitly out of scope; do not revisit.
2. SC-UI-1 — Blurred album art background (rendering pipeline review needed; downscale-trick via `DecodePixelWidth(8)` → `ImageBrush` is the low-scope path; Composition GaussianBlur is higher quality but requires compositor access probe)
3. SC-FLY-1 — Hover flyout panel (XAML `Popup` anchored to widget, shows on `PointerEntered`/`PointerExited`; renders full metadata — large art, untruncated title/artist, duration, playback speed, chapter info; gate: positioning probe needed to confirm popup escapes taskbar bounds upward in injected XAML island; lyrics pane is separable additive scope via SC-HT-1)
4. SC-HT-1 — LRC lyrics overlay (significant scope increase; natural fit as flyout content tier 2 after SC-FLY-1)
5. SC-GR-1 — FFT audio visualizer (process compatibility audit required)

**v1.5 / post-release maybe:**
- **Chrome Extension companion** — A Chrome extension + Native Messaging host that relays richer Media Session state (chapter metadata, `setPositionState` data Libby doesn't push to SMTC) to the mod via named pipe or shared memory. Motivation: Libby is a Chrome PWA; its SMTC ceiling is whatever it publishes via `navigator.mediaSession`, and it doesn't call `setPositionState()`. A companion extension is the only clean path past that ceiling. Scope: extension + native host exe + IPC layer in mod — three moving parts, non-trivial install story. Revisit only after core mod is stable at v1.x.
