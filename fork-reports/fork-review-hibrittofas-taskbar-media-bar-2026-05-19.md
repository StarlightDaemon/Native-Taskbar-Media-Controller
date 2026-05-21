# Fork Review: Taskbar Media Bar

| Field | Value |
|---|---|
| Fork name | Taskbar Media Bar |
| Author | HibritTofas |
| Source file | `/forks/hibrittofas/taskbar-media-bar/taskbar-media-bar.wh.cpp` |
| Baseline file | `/forks/og_Hashah2311_taskbar-music-lounge.wh/og_Hashah2311_taskbar-music-lounge.wh.cpp` |
| Review date | 2026-05-19 |
| MANIFEST flags | None |

## Baseline Reference

See **Baseline Identification** section in `fork-reports/synthesis-2026-05-19.md` for full baseline characterization.

---

## Diff Analysis

This fork is the second-largest in the set (~2500+ lines). It extends the baseline with synchronized lyrics, a custom context menu, chameleon palette theming, per-app volume control, and multiple additional HWNDs. The baseline's single-window design is expanded to five: main media window, popup, context menu, sub-menu, and lyrics window.

### Additions

#### Multi-Window Architecture

| Unit | Lines | Description |
|---|---|---|
| `g_hPopupWindow` | ~210 | HWND for a popup detail view |
| `g_hContextMenu` | ~212 | HWND for a custom-rendered context menu |
| `g_hSubMenu` | ~214 | HWND for a context menu submenu |
| `g_hLyricsWindow` | ~216 | HWND for the lyrics overlay window |
| `LyricsWndProc` | ~1600–1800 | Window procedure for the lyrics window; renders animated dots (loading state) and line crossfade transitions |

#### Lyrics Subsystem

| Unit | Lines | Description |
|---|---|---|
| `FetchLyricsAsync()` | ~900–1050 | WinHTTP-based async lyrics fetch. Primary source: Musixmatch API (requires key). Fallback: LRCLIB (open, no key needed). Runs on a background thread. |
| `ParseLRC(const std::wstring&)` | ~1060–1130 | Parses LRC format (`[MM:SS.xx]lyric line`) into a `std::vector<LyricLine>` of timestamped lines. |
| `GetLyricLines()` | ~1140–1160 | Returns the current lyric line based on `positionMs`. |
| `LyricLine` struct | ~175 | `int64_t timestamp_ms; std::wstring text` |
| `g_LyricsSlideX` | ~240 | Float tracking horizontal slide offset for lyric line transitions |
| `@compilerOptions -lwinhttp` | ~5 | Required for `WinHTTP` linkage |

#### Album Art Palette / Chameleon Theming

| Unit | Lines | Description |
|---|---|---|
| `GetAlbumPalette()` | ~700–800 | 64-bucket (4×4×4 RGB) quantization of the album art bitmap. Returns `AlbumPalette` with primary and secondary colors. |
| `AlbumPalette` struct | ~180 | `Color primary; Color secondary` |
| Chameleon theming | ~1900–2100 | Uses `AlbumPalette` colors to drive background gradient and text color. `LinearGradientBrush` from primary to secondary. |
| `AdaptiveChameleonText` | ~2110–2180 | Computes text color (white or black) based on the background luminance from `AlbumPalette`. |
| Six visual themes | ~185–195 | `ColorTheme` enum: Default, Chameleon, Dark, Light, Transparent, Custom |

#### Playback State Extensions

| Unit | Lines | Description |
|---|---|---|
| `MediaState::progressRatio`, `hasProgress` | ~165–166 | Progress ratio (0.0–1.0) and availability flag |
| `MediaState::positionMs` | ~167 | Current playback position in milliseconds |
| `MediaState::sourceAppId` | ~168 | AUMID for `BringSourceAppToFront` (same pattern as Messij v1.x) |
| `MediaState::isShuffle` | ~169 | Shuffle state from GSMTC |
| `MediaState::repeatMode` | ~170 | Repeat mode from GSMTC |
| Spotify-priority session logic | ~620–660 | Prefers Spotify session when multiple sessions exist; falls back to current if Spotify not present |
| `IsForegroundFullscreen()` | ~480–510 | Monitor-coverage fullscreen check (same pattern as kevinoe fork, arrived at independently) |

#### Input / UX

| Unit | Lines | Description |
|---|---|---|
| `BringSourceAppToFront()` | ~840–900 | Same AUMID + exe-hint EnumWindows pattern as Messij v1.x fork |
| Custom context menu | ~2300–2500 | Owner-drawn context menu via `g_hContextMenu` HWND with sub-menu support; actions: Play/Pause, Next, Prev, Volume, Theme, Lyrics toggle |
| Per-app volume control | ~1200–1350 | `IAudioSessionManager2` → `ISimpleAudioVolume` per-session volume. Volume slider in context menu. |
| Auto-hide with delay | ~1380–1440 | Hides widget after configurable idle period; `g_LyricsSlideX` synced to auto-hide animation so lyrics don't detach |
| Mini mode | ~1450–1510 | Collapses to art-only when width ≤ height + 10 (same geometry as Uiisland fork) |

#### Persistence

| Unit | Lines | Description |
|---|---|---|
| `SaveUIState()` / `LoadUIState()` | ~1520–1590 | Saves/restores theme, volume, lyrics-on state to `HKCU\Software\taskbar-media-bar` via direct registry API (not `Wh_SetStringValue`) |

#### WinEvent Hooks

| Unit | Lines | Description |
|---|---|---|
| `g_hForegroundHook` | ~220 | `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)` — detects app focus changes for auto-hide |
| `g_hMoveSizeHook` | ~222 | `SetWinEventHook(EVENT_SYSTEM_MOVESIZEEND)` — repositions widget after window moves |
| `g_hTaskbarMoveHook` | ~224 | `SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE)` — same as baseline's taskbar position sync |

#### Smooth Progress

| Unit | Lines | Description |
|---|---|---|
| Smooth progress interpolation | ~1600–1650 | Interpolates `progressRatio` between poll ticks using elapsed time to avoid jumpy progress bar updates |

### Subtractions

| Unit | Description |
|---|---|
| No settings removed from baseline | All baseline settings retained |
| Single `g_MediaState` | Retained (no multi-session array; Spotify-priority picks one session) |

### Modifications

| Unit | Change |
|---|---|
| `UpdateMediaInfo()` | Adds Spotify-priority session selection; fetches isShuffle, repeatMode, positionMs; triggers `FetchLyricsAsync` on track change |
| `DrawMediaPanel()` | Renders chameleon gradient background; progress bar; shuffle/repeat indicators |
| `WM_RBUTTONUP` | Opens custom context menu |
| `WM_LBUTTONDBLCLK` | Calls `BringSourceAppToFront()` |
| Version | v4.0.1 → v1.2 (renumbered by HibritTofas) |

---

## Attribution

| Change | Author |
|---|---|
| All changes above | HibritTofas (declared `@author HibritTofas` in file header) |

No secondary contributors cited, though `BringSourceAppToFront` pattern is structurally identical to Messij v1.x.

---

## Code Quality Signals

| Signal | Label | Notes |
|---|---|---|
| LRC lyrics fetch with dual-source fallback (Musixmatch + LRCLIB) is a well-designed resilience pattern | Positive | LRCLIB is a free, open API; degradation from paid to free source is correct |
| `ParseLRC()` with timestamped lines is clean and standard | Positive | LRC is the de-facto format; the parser handles both MM:SS.xx and MM:SS.xxx formats |
| `GetAlbumPalette()` 64-bucket quantization is a reasonable quality/performance tradeoff | Positive | 4×4×4 buckets avoids the per-pixel GDI+ approach used by Uiisland; faster but less precise |
| Five HWNDs (main, popup, context, submenu, lyrics) substantially increase resource management complexity | Negative (High) | Multiple windows require consistent WM_DESTROY, WM_NCDESTROY, and cleanup paths; the reviewed code has inconsistencies in lifecycle management |
| Spotify-priority session selection is a hard incompatibility with non-Spotify sessions (same issue as memeri121) | Negative | Users with VLC or browser audio get an inferior experience |
| `SaveUIState()` writes directly to `HKCU\Software\taskbar-media-bar` via registry API rather than `Wh_SetStringValue` | Negative (Low) | Bypasses Windhawk's settings abstraction; leaves registry keys behind on uninstall |
| Smooth progress interpolation between poll ticks is a UX improvement absent from all other forks | Positive | Progress bar appears fluid rather than jumping every 1000ms |
| `g_LyricsSlideX` synced to auto-hide animation prevents lyrics detaching from the widget during hide | Positive | Subtle but correct UX detail |
| `AdaptiveChameleonText` luminance-based text color follows the same principle as Uiisland's `isDarkCover` | Positive | Independent convergence on a correct approach |
| `@compilerOptions -lwinhttp` dependency means the mod will fail to compile without WinHTTP headers and linking | Neutral | Required for the lyrics feature; acceptable for a mod that targets this capability |

---

## Synthesis Candidates from This Fork

### SC-HT-1: LRC lyrics subsystem (FetchLyricsAsync + ParseLRC + LyricsWndProc)
- **Signal:** Recommended
- **Class:** MULTI-UNIT-INTEGRATION
- **Recommended model:** Gemini 3 Pro (high), Sonnet 4.6
- **Seed:** Port `FetchLyricsAsync()`, `ParseLRC()`, `GetLyricLines()`, `LyricLine` struct, and `LyricsWndProc` from `hibrittofas/taskbar-media-bar/taskbar-media-bar.wh.cpp`. Target: a separate lyrics overlay window that shows the current lyric line, synced to `positionMs`. Add `-lwinhttp` to `@compilerOptions`. Primary source: LRCLIB (no API key). Musixmatch integration optional (requires key in settings). Add `positionMs` to `MediaState` (see also SC-HT-3). Success criterion: lyrics display and advance correctly on a track with available LRC data; lyrics window is destroyed cleanly in `WM_DESTROY`. Attribute to HibritTofas.

### SC-HT-2: GetAlbumPalette (64-bucket color quantization)
- **Signal:** Recommended
- **Class:** TARGETED-PORT
- **Recommended model:** Gemini 3 Pro (low), Sonnet 4.5
- **Seed:** Port `GetAlbumPalette()` and `AlbumPalette` struct from `hibrittofas/taskbar-media-bar/taskbar-media-bar.wh.cpp`. Target: call after album art loads in `UpdateMediaInfo`; use primary/secondary colors for chameleon background gradient. Note: Uiisland's SC-UI-2 uses 1×1 downscale for luminance only; this candidate provides both primary and secondary palette for gradient use. Choose between them based on desired visual output. Attribute to HibritTofas.

### SC-HT-3: MediaState shuffle/repeat/positionMs fields
- **Signal:** Consider
- **Class:** MECHANICAL
- **Recommended model:** Gemini 3 Flash, Claude Haiku 4.5
- **Seed:** Add `isShuffle`, `repeatMode`, and `positionMs` fields to `MediaState` in a successor. Source: `hibrittofas/taskbar-media-bar/taskbar-media-bar.wh.cpp`. Target: populate in `UpdateMediaInfo()` from GSMTC `GetPlaybackInfo()` and `GetTimelineProperties()`. `positionMs` is a prerequisite for SC-HT-1. Success criterion: fields populated correctly; `positionMs` advances between poll ticks (use smooth interpolation from SC-HT-5 for display). Attribute to HibritTofas.

### SC-HT-4: Smooth progress interpolation
- **Signal:** Consider
- **Class:** MECHANICAL
- **Recommended model:** Gemini 3 Flash, Claude Haiku 4.5
- **Seed:** Port the smooth progress interpolation logic from `hibrittofas/taskbar-media-bar/taskbar-media-bar.wh.cpp`. Uses elapsed time since last poll to interpolate `progressRatio` between ticks, making the progress bar appear fluid. Target: apply in `IDT_ANIMATION` frame tick. Requires `positionMs` in `MediaState` (SC-HT-3). Success criterion: progress bar advances smoothly at ~60fps rather than jumping at 1000ms poll intervals. Attribute to HibritTofas.

### SC-HT-5: Per-app volume control (IAudioSessionManager2 + ISimpleAudioVolume)
- **Signal:** Consider
- **Class:** TARGETED-PORT
- **Recommended model:** Gemini 3 Pro (low), Sonnet 4.5
- **Seed:** Port the `IAudioSessionManager2` → `ISimpleAudioVolume` per-app volume control from `hibrittofas/taskbar-media-bar/taskbar-media-bar.wh.cpp`. Target: expose volume control via scroll wheel (or settings). Note: Cinabutts fork (SC-CI-1) provides `IAudioMeterInformation` for peak reading — these are complementary APIs from the same COM subsystem. Success criterion: scroll wheel adjusts the media app's session volume independently of master volume. Attribute to HibritTofas.

---

## Flags

- **Direct registry writes:** `SaveUIState()` and `LoadUIState()` write to `HKCU\Software\taskbar-media-bar` via raw registry API, bypassing `Wh_SetStringValue`. If porting, replace with `Wh_SetStringValue`/`Wh_GetStringValue` to avoid leaving orphaned registry keys on uninstall.
- **Spotify-priority session logic:** If porting any session-selection code, do NOT port the Spotify-preference filter. Use the current GSMTC session or a user-configurable preference instead.

---

## Appendix

- Tools used: Read (offset/limit in chunks of 600 lines due to file size), diff-by-inspection against baseline summary
- Approximate line count: ~2500+ lines
- No skips
