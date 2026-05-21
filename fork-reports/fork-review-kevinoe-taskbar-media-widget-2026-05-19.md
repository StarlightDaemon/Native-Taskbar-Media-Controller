# Fork Review: Taskbar Media Widget

| Field | Value |
|---|---|
| Fork name | Taskbar Media Widget |
| Author | kevinoe |
| Source file | `/forks/kevinoe/taskbar-media-widget/taskbar-media-widget.wh.cpp` |
| Baseline file | `/forks/og_Hashah2311_taskbar-music-lounge.wh/og_Hashah2311_taskbar-music-lounge.wh.cpp` |
| Review date | 2026-05-19 |
| MANIFEST flags | None |

## Baseline Reference

See **Baseline Identification** section in `fork-reports/synthesis-2026-05-19.md` for full baseline characterization.

---

## Diff Analysis

This fork is a substantial structural rewrite. The rendering pipeline is replaced from `SetLayeredWindowAttributes` to `UpdateLayeredWindow(ULW_ALPHA)` for per-pixel alpha. Settings are completely redesigned. Taskbar hook is removed entirely.

### Additions

| Unit | Lines | Description |
|---|---|---|
| `RenderLayeredWindow()` | ~480–560 | New per-pixel alpha render path using `UpdateLayeredWindow(ULW_ALPHA)`. Constructs a DIB section, renders GDI+ output to it, and calls `UpdateLayeredWindow` with a full `BLENDFUNCTION`. Replaces `SetLayeredWindowAttributes`. |
| `DrawMediaPanel(Graphics&, int, int)` | Signature change | Draw function now takes `Graphics` reference and explicit width/height; called by `RenderLayeredWindow`. |
| `ParseRgbaColorSetting(const wchar_t*)` | ~240–260 | Parses `"R,G,B,A"` string format into a GDI+ `Color`. Enables per-channel alpha in color settings. |
| `CreateRoundedRectPath(GraphicsPath&, RectF, float)` | ~270–285 | Creates a GDI+ `GraphicsPath` for a rounded rectangle from a `RectF` and corner radius float. Used for clipping and fills. |
| `IsForegroundWindowFullscreen()` | ~290–315 | Checks if the foreground window covers the entire monitor using monitor-coverage calculation (foreground window rect intersection with monitor work area). More precise than baseline's style-flag approach. |
| `BackgroundColor` setting | ~118 | `"R,G,B,A"` string format; default `"0,0,0,200"`. Replaces `BgOpacity` integer. |
| `TextColor` setting | ~121 | `"R,G,B,A"` string format; default `"255,255,255,255"`. Replaces integer hex color. |
| `TrackProgressColor` setting | ~124 | `"R,G,B,A"` string format; default `"255,255,255,128"`. Color for the track progress bar. |
| `FontWeight` setting | ~127 | `"Regular"` or `"Bold"` string. |
| `ScrollLongText` setting | ~130 | Boolean; controls whether long text scrolls. |
| `HorizontalAlignment` setting | ~133 | `"Left"`, `"Center"`, or `"Right"` string for text alignment. |
| `CornerRadius` setting | ~136 | Float; controls widget corner rounding via `CreateRoundedRectPath`. |
| `ShowAlbumArt` setting | ~139 | Boolean toggle. |
| `ShowTrackProgress` setting | ~142 | Boolean toggle for the progress bar row. |
| Track progress bar drawing | ~820–850 | Draws a thin progress bar below the text row using `MediaState` timeline fields (`positionTicks`, `endTicks`). |
| GSMTC timeline fetch in `UpdateMediaInfo()` | ~650–680 | Calls `GetTimelineProperties()` to populate `positionTicks`, `startTicks`, `endTicks`. Used for `ShowTrackProgress`. |
| `UpdateMediaInfo()` returns `bool changed` | ~600 | Returns whether any state changed; caller uses the result to skip unnecessary repaints. |

### Subtractions

| Unit | Description |
|---|---|
| `ButtonScale` setting | Removed entirely |
| `HideFullscreen` setting | Removed; fullscreen hiding is always-on via `IsForegroundWindowFullscreen()` |
| `IdleTimeout` setting | Removed |
| `AutoTheme` setting | Removed; no light/dark detection |
| `BgOpacity` setting (integer) | Replaced by `BackgroundColor` "R,G,B,A" string |
| `IsSystemLightMode()` | Removed (no AutoTheme) |
| `GetCurrentTextColor()` | Removed (text color from setting only) |
| `UpdateAppearance()` | Removed (no acrylic/DWM setup; `ACCENT_DISABLED`) |
| `RegisterTaskbarHook()`, `g_TaskbarHook`, `g_TaskbarCreatedMsg` | Entirely removed; no taskbar event hook |
| `TaskbarEventProc`, `IsTaskbarWindow()` | Removed with hook |
| `WM_APP+10` handler | Removed (no taskbar-moved event) |
| `SetCurrentProcessExplicitAppUserModelID` call | Removed |
| `SetLayeredWindowAttributes` path | Replaced by `UpdateLayeredWindow` |

### Modifications

| Unit | Change |
|---|---|
| Window creation | `DWMWCP_DONOTROUND` set explicitly; `ACCENT_DISABLED` (no acrylic); `ShowWindow(hwnd, SW_HIDE)` in `WM_CREATE` — widget auto-shows only when media becomes available |
| `SetWindowPos` z-order | Uses `HWND_TOP` instead of baseline's `HWND_TOPMOST` |
| `PanelHeight` default | 36 (baseline: 48) |
| Color settings format | All colors use `"R,G,B,A"` string format via `ParseRgbaColorSetting` |
| Version | v4.0.1 → v1.3 (renumbered by kevinoe) |

---

## Attribution

| Change | Author |
|---|---|
| All changes above | kevinoe (declared `@author kevinoe` in file header) |

No secondary contributors cited.

---

## Code Quality Signals

| Signal | Label | Notes |
|---|---|---|
| `UpdateLayeredWindow(ULW_ALPHA)` enables true per-pixel alpha transparency — a genuine capability upgrade over baseline's `SetLayeredWindowAttributes` | Positive | Allows non-rectangular widget shapes, per-pixel shadows, smooth edge blending |
| `ParseRgbaColorSetting` gives users per-channel alpha control over all colors | Positive | More expressive than separate color + opacity settings |
| `UpdateMediaInfo()` returning `bool changed` is a correct optimization to avoid unnecessary repaints | Positive | Simple, no-overhead improvement |
| `IsForegroundWindowFullscreen()` using monitor-coverage is more accurate than style-flag checks | Positive | Avoids false-positives from windows that set WS_EX_TOPMOST but don't cover the screen |
| Removal of the taskbar hook means the widget does not reanchor when the taskbar moves or reappears | Negative | Same regression as memeri121 fork; widget can drift or fail to appear on taskbar crash/restart |
| `HWND_TOP` (non-topmost) instead of `HWND_TOPMOST` means widget can be occluded by normal windows | Negative | May be intentional for a less-intrusive widget, but differs from baseline behavior |
| Removing `ButtonScale`, `HideFullscreen`, `IdleTimeout`, `AutoTheme` removes user-visible features without replacement | Negative | Not necessarily wrong for a focused reimplementation, but users migrating from baseline lose functionality |
| `ShowWindow(SW_HIDE)` in WM_CREATE with auto-show on media is a cleaner UX pattern than baseline's always-visible approach | Positive | Widget does not occupy taskbar space when nothing is playing |
| `"R,G,B,A"` string format for color settings is non-standard vs. Windhawk's typical hex integer format | Neutral | Requires `ParseRgbaColorSetting`; incompatible with settings from other forks |

---

## Synthesis Candidates from This Fork

### SC-KV-1: RenderLayeredWindow with UpdateLayeredWindow(ULW_ALPHA)
- **Signal:** Recommended
- **Class:** MULTI-UNIT-INTEGRATION
- **Recommended model:** Gemini 3 Pro (high), Sonnet 4.6
- **Seed:** Port the `RenderLayeredWindow()` function and DIB-section render pipeline from `kevinoe/taskbar-media-widget/taskbar-media-widget.wh.cpp`. Target: replace `SetLayeredWindowAttributes` with `UpdateLayeredWindow(ULW_ALPHA)`. Requires updating `WS_EX_LAYERED` window creation and all `ShowWindow`/`SetWindowPos` calls to preserve the layered state. Success criterion: widget renders with per-pixel alpha; rounded corners show true transparency rather than solid edges. Attribute to kevinoe.

### SC-KV-2: IsForegroundWindowFullscreen monitor-coverage check
- **Signal:** Recommended
- **Class:** TARGETED-PORT
- **Recommended model:** Gemini 3 Pro (low), Sonnet 4.5
- **Seed:** Port `IsForegroundWindowFullscreen()` from `kevinoe/taskbar-media-widget/taskbar-media-widget.wh.cpp`. Target: replace baseline's style-flag fullscreen detection in the WM_TIMER polling path. Success criterion: widget hides when a true fullscreen window is active; does not false-positive on maximized non-fullscreen windows. Attribute to kevinoe.

### SC-KV-3: CreateRoundedRectPath utility
- **Signal:** Consider
- **Class:** MECHANICAL
- **Recommended model:** Gemini 3 Flash, Claude Haiku 4.5
- **Seed:** Port `CreateRoundedRectPath(GraphicsPath&, RectF, float)` from `kevinoe/taskbar-media-widget/taskbar-media-widget.wh.cpp`. Target: replace the baseline's `AddRoundedRect` inline logic with this cleaner utility. Success criterion: rounded rectangle clipping and fill paths are drawn correctly with a configurable radius. Attribute to kevinoe.

### SC-KV-4: Track progress bar (ShowTrackProgress + GSMTC timeline)
- **Signal:** Consider
- **Class:** TARGETED-PORT
- **Recommended model:** Gemini 3 Pro (low), Sonnet 4.5
- **Seed:** Port the GSMTC timeline fetch (`GetTimelineProperties`) from `UpdateMediaInfo()` and the progress bar drawing from `DrawMediaPanel` in `kevinoe/taskbar-media-widget/taskbar-media-widget.wh.cpp`. Target: add a `ShowTrackProgress` setting and a thin progress bar below the text row. The Spotify-only seek bar from SC-SP-1 (memeri121) is the more complete implementation; this candidate is an alternative if full drag-seek is not desired. Success criterion: progress bar advances as playback progresses; no drag interaction. Attribute to kevinoe.

### SC-KV-5: UpdateMediaInfo returning bool changed
- **Signal:** Consider
- **Class:** MECHANICAL
- **Recommended model:** Gemini 3 Flash, Claude Haiku 4.5
- **Seed:** Change `UpdateMediaInfo()` to return `bool` indicating whether any state changed. In the WM_TIMER handler, skip `InvalidateRect` if the return value is false. Source: `kevinoe/taskbar-media-widget/taskbar-media-widget.wh.cpp`. Success criterion: no visible change; CPU overhead on idle tick reduced. Attribute to kevinoe.

---

## Flags

None.

---

## Appendix

- Tools used: Read (offset/limit), diff-by-inspection against baseline summary
- Approximate line count: ~1230 lines
- No skips
