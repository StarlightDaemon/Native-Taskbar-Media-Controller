# Fork Review: Taskbar Spotify Widget

| Field | Value |
|---|---|
| Fork name | Taskbar Spotify Widget |
| Author | memeri121 |
| Source file | `/forks/memeri121/taskbar-spotify-widget/taskbar-spotify-widget.wh.cpp` |
| Baseline file | `/forks/og_Hashah2311_taskbar-music-lounge.wh/og_Hashah2311_taskbar-music-lounge.wh.cpp` |
| Review date | 2026-05-19 |
| MANIFEST flags | None |

## Baseline Reference

See **Baseline Identification** section in `fork-reports/synthesis-2026-05-19.md` for full baseline characterization.

---

## Diff Analysis

### Additions

| Unit | Lines | Description |
|---|---|---|
| `IsSpotifySession()` | ~210–220 | Checks if a GSMTC session's SourceAppUserModelId contains "Spotify" |
| `GetSpotifySession()` | ~223–240 | Iterates GSMTC sessions and returns the Spotify session handle; falls back to current session |
| `MediaState::canSeek` (field) | ~152 | Whether the session supports seeking |
| `MediaState::startTicks`, `endTicks`, `positionTicks` (fields) | ~153–155 | Timeline data from GSMTC TimelineProperties |
| `MediaState::minSeekTicks`, `maxSeekTicks` (fields) | ~156–157 | Seek range from GSMTC |
| `bool g_IsDraggingSeek` | ~198 | True while user is dragging the seek bar |
| `float g_DragSeekRatio` | ~199 | Current drag position as 0.0–1.0 ratio |
| `SeekToRatio(float)` | ~380–400 | Sends a seek command to GSMTC at a given ratio; uses `TryChangePlaybackPositionAsync` |
| `SeekBySeconds(int)` | ~403–415 | Seeks forward or backward by a fixed number of seconds |
| `GetProgressBarRect()` | ~418–428 | Returns the RECT for the seek bar within the panel |
| `SeekRatioFromX(int)` | ~430–440 | Converts an x-coordinate to a seek ratio |
| `HWND g_hTooltip` | ~203 | Tooltip HWND for seek time display |
| `CreateTooltip()` | ~443–465 | Creates a TOOLTIPS_CLASS tooltip window with `TTF_IDISHWND` |
| `UpdateTooltipText()` | ~468–490 | Formats elapsed/total time as "MM:SS / MM:SS" and updates tooltip text |
| `HideTooltip()` | ~493–500 | Activates tooltip popup and triggers immediate repaint |
| `g_CurrentAlpha`, `g_TargetAlpha` | ~205–206 | Current and target window alpha for fade animation |
| `EnsureFadeTimer()`, `StopFadeTimer()` | ~510–530 | Start/stop `IDT_FADE` (timer 1003, 12ms) for alpha fade transitions |
| `ApplyLayeredAlpha(BYTE)` | ~533–540 | Calls `SetLayeredWindowAttributes(LWA_ALPHA)` |
| `SetWidgetVisible(bool)` | ~543–560 | Sets `g_TargetAlpha` and starts fade timer; encapsulates show/hide logic |
| `IDT_FADE` (timer 1003, 12ms) | ~600 | Drives alpha fade animation |
| `g_SeekbarAnim` (struct) | ~208–215 | Seek bar animation state: current width progress |
| `EnsureSeekbarAnimTimer()`, `StopSeekbarAnimTimer()` | ~563–580 | Start/stop `IDT_SEEKBAR` (timer 1004, 16ms) |
| `IDT_SEEKBAR` (timer 1004, 16ms) | ~602 | Drives seek bar fill animation |
| `GetArtRect()`, `GetControlsRect()`, `GetTextRect()`, `GetContentRect()` | ~280–340 | Layout helper functions returning `RECT` for each UI zone |
| `GetButtonRect(int)` | ~343–360 | Returns the RECT for a specific button (0=prev, 1=play, 2=next) within `GetControlsRect()` |
| `DrawPrevIcon()`, `DrawPlayPauseIcon()`, `DrawNextIcon()` | ~720–800 | Separate per-button icon draw functions using GDI+ path operations |
| `DrawButtonBg()` | ~803–820 | Draws the circular button background for hover state |
| `DrawRoundedImage()` | ~823–840 | Draws bitmap with `GraphicsPath` rounded clipping (replaces inline rounded-clip in baseline) |
| `FillRoundedRect()` | ~843–855 | Fills a rounded rectangle — utility used for seek bar background |
| `bool UseBlackBackground` setting | ~120 | Force black background instead of AutoTheme (default true) |
| `bool ShowAlbumArt` setting | ~125 | Toggle album art display |
| `bool ShowSeekbar` setting | ~128 | Toggle seek bar visibility |
| `bool CompactMode` setting | ~131 | Compact layout (smaller font/elements) |
| `bool OpenSpotifyOnSingleClick` setting | ~134 | Single click opens Spotify app (default false) |
| `ControlsSide` setting (left/right) | ~137 | Controls placement left or right of text |
| `g_LastRenderedText` | ~200 | Tracks last rendered title to detect track changes and reset scroll position |
| `CS_DBLCLKS` added to window class style | ~1050 | Enables `WM_LBUTTONDBLCLK` message delivery |
| `WM_LBUTTONDBLCLK` → `OpenSpotify()` | ~1120 | Double-click opens Spotify via shell `spotify:` URI scheme |
| Shift+scroll → seek, plain scroll → volume | ~1130–1150 | `WM_MOUSEWHEEL` branching: Shift+scroll calls `SeekBySeconds(±10)`, plain scroll adjusts volume |
| Poll timer 900ms (vs 1000ms baseline) | ~1060 | Faster polling interval |
| `g_ScrollWait = 75` (vs 60 baseline) | ~196 | Slightly longer scroll pause before animation resumes |

### Subtractions

| Unit | Description |
|---|---|
| `RegisterTaskbarHook()`, `g_TaskbarHook`, `g_TaskbarCreatedMsg` | Entirely removed; no taskbar event hook |
| `SetCurrentProcessExplicitAppUserModelID` call in ModInit | Removed |
| `IsTaskbarWindow()` | Removed (no hook, no taskbar tracking) |
| `TaskbarEventProc` | Removed |
| `IsSystemLightMode()` / `AutoTheme` | Replaced by `UseBlackBackground` setting |
| `WM_APP+10` message handler (taskbar moved) | Removed |
| `WM_DESTROY` hook cleanup | No hook to clean up |

### Modifications

| Unit | Change |
|---|---|
| `UpdateMediaInfo()` | Uses `GetSpotifySession()` instead of `g_sessionManager.GetCurrentSession()` |
| `DrawMediaPanel()` | Completely rewritten: layout via `GetArtRect`/`GetControlsRect`/`GetTextRect`; draws seek bar below controls; calls new per-button draw functions |
| `UpdateAppearance()` | Uses `UseBlackBackground` logic instead of `AutoTheme`; no light/dark registry check |
| `WM_LBUTTONUP` | Checks for hit on button rects via `GetButtonRect()`; also checks seek bar hit for `SeekToRatio` |
| `WM_MOUSEMOVE` | Updates hover state for seek bar; calls `UpdateTooltipText()` when hovering seek |
| Settings defaults | `PanelWidth=320`, `PanelHeight=52` (baseline: 300/48); `BgOpacity=200` |
| Timer IDs | Adds `IDT_FADE=1003` and `IDT_SEEKBAR=1004` alongside baseline `IDT_POLL_MEDIA=1001` and `IDT_ANIMATION=1002` |

---

## Attribution

| Change | Author |
|---|---|
| All changes above | memeri121 (declared `@author memeri121` in file header) |

No secondary contributors cited.

---

## Code Quality Signals

| Signal | Label | Notes |
|---|---|---|
| Seek bar with drag + tooltip is a high-quality UX addition absent from all other forks | Positive | Full drag, ratio conversion, `TryChangePlaybackPositionAsync`, and time tooltip are all coherent |
| Layout helper functions (`GetArtRect` etc.) are a structural improvement over inline coordinate calculations | Positive | Easier to maintain and modify |
| Separate icon draw functions improve readability over baseline's inline draw paths | Positive | Clear separation of concerns |
| Fade animation for widget visibility is smoother than baseline's abrupt `ShowWindow` | Positive | Uses `SetLayeredWindowAttributes` correctly on each frame tick |
| Spotify-only filtering is a hard incompatibility with universal GSMTC design | Negative | The mod will not display non-Spotify sessions at all; a user with VLC or browser audio would see nothing |
| Removal of taskbar hook means the widget does not reposition when the taskbar moves or appears after a crash | Negative | Widget can drift off-screen or appear above a hidden taskbar; a known issue in the baseline that this fork makes worse |
| `WM_CREATE` does not call `RegisterTaskbarHook`; no `g_TaskbarCreatedMsg` handler | Negative | Permanent regression vs. baseline for taskbar auto-anchor |
| `g_ScrollWait = 75` (longer than baseline's 60) causes a slightly more sluggish scroll resume | Neutral | Minor preference difference |
| `CS_DBLCLKS` required for correct `WM_LBUTTONDBLCLK` delivery — missing in baseline | Positive | Baseline would silently eat double-clicks; this fork correctly adds the style |

---

## Synthesis Candidates from This Fork

### SC-SP-1: Seek bar with drag + GSMTC `TryChangePlaybackPositionAsync`
- **Signal:** Recommended
- **Class:** MULTI-UNIT-INTEGRATION
- **Recommended model:** Gemini 3 Pro (high), Sonnet 4.6
- **Seed:** Port the seek bar subsystem from `memeri121/taskbar-spotify-widget/taskbar-spotify-widget.wh.cpp`: `MediaState` timeline fields (canSeek, startTicks/endTicks/positionTicks), `GetProgressBarRect()`, `SeekRatioFromX()`, `SeekToRatio()`, drag state vars, and seek bar drawing in `DrawMediaPanel`. Target: add a seek bar below the controls row in a successor. The Spotify-only session filter must NOT be ported — seek should be gated on `canSeek` being true for the current session. Success criterion: dragging the seek bar updates GSMTC position; no drag leaves visual state unaltered. Attribute to memeri121.

### SC-SP-2: Seek time tooltip
- **Signal:** Recommended
- **Class:** TARGETED-PORT
- **Recommended model:** Gemini 3 Pro (low), Sonnet 4.5
- **Seed:** Port `CreateTooltip()`, `UpdateTooltipText()`, `HideTooltip()`, and `g_hTooltip` from `memeri121/taskbar-spotify-widget/taskbar-spotify-widget.wh.cpp`. Target: show elapsed/total time on seek bar hover. Depends on SC-SP-1. Success criterion: tooltip appears over seek bar during hover with "MM:SS / MM:SS" format. Attribute to memeri121.

### SC-SP-3: Layout helper functions (GetArtRect / GetControlsRect / GetTextRect / GetButtonRect)
- **Signal:** Recommended
- **Class:** TARGETED-PORT
- **Recommended model:** Gemini 3 Pro (low), Sonnet 4.5
- **Seed:** Port `GetArtRect()`, `GetControlsRect()`, `GetTextRect()`, `GetContentRect()`, and `GetButtonRect(int)` from `memeri121/taskbar-spotify-widget/taskbar-spotify-widget.wh.cpp`. Target: replace inline coordinate arithmetic in `DrawMediaPanel` and mouse handlers in a successor. Success criterion: all existing layout regions are preserved; hit-testing uses the same geometry. Attribute to memeri121.

### SC-SP-4: Widget fade animation (SetWidgetVisible / EnsureFadeTimer)
- **Signal:** Consider
- **Class:** TARGETED-PORT
- **Recommended model:** Gemini 3 Pro (low), Sonnet 4.5
- **Seed:** Port `g_CurrentAlpha`, `g_TargetAlpha`, `EnsureFadeTimer()`, `StopFadeTimer()`, `ApplyLayeredAlpha()`, and `SetWidgetVisible()` from `memeri121/taskbar-spotify-widget/taskbar-spotify-widget.wh.cpp`. Target: replace abrupt `ShowWindow` calls with alpha fade transitions. Success criterion: widget fades in/out on media appear/disappear. Attribute to memeri121.

### SC-SP-5: CS_DBLCLKS window class style
- **Signal:** Recommended
- **Class:** MECHANICAL
- **Recommended model:** Gemini 3 Flash, Claude Haiku 4.5
- **Seed:** Add `CS_DBLCLKS` to the window class registration in `Wh_ModInit` (or equivalent) in a successor. Source: `memeri121/taskbar-spotify-widget/taskbar-spotify-widget.wh.cpp`. Success criterion: `WM_LBUTTONDBLCLK` messages are delivered to the WndProc. This is a one-line fix required for any double-click feature to work. Attribute to memeri121.

### SC-SP-6: SeekBySeconds on scroll (Shift+scroll)
- **Signal:** Consider
- **Class:** MECHANICAL
- **Recommended model:** Gemini 3 Flash, Claude Haiku 4.5
- **Seed:** Port the `WM_MOUSEWHEEL` branch from `memeri121/taskbar-spotify-widget/taskbar-spotify-widget.wh.cpp` that checks for `MK_SHIFT` and calls `SeekBySeconds(±10)`. Requires SC-SP-1. Success criterion: Shift+scroll advances or rewinds playback position by 10 seconds. Attribute to memeri121.

---

## Flags

None.

---

## Appendix

- Tools used: Read (offset/limit), diff-by-inspection against baseline summary
- Approximate line count: ~1603 lines
- No skips
