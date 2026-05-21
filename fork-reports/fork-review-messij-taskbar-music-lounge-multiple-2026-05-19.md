# Fork Review: Taskbar Music Lounge Multiple

| Field | Value |
|---|---|
| Fork name | Taskbar Music Lounge Multiple |
| Author | Messij |
| Source file | `/forks/messij/taskbar-music-lounge-multiple/mod.wh.cpp` |
| Baseline file | `/forks/og_Hashah2311_taskbar-music-lounge.wh/og_Hashah2311_taskbar-music-lounge.wh.cpp` |
| Review date | 2026-05-19 |
| MANIFEST flags | Duplicate filename conflict (row 9 is same physical file as row 3 — treated as one fork) |

## Baseline Reference

See **Baseline Identification** section in `fork-reports/synthesis-2026-05-19.md` for full baseline characterization.

---

## Diff Analysis

### Additions

| Unit | Lines | Description |
|---|---|---|
| `MediaState g_MediaStates[10]` | ~180 | Array of 10 MediaState structs replacing single `g_MediaState`; enables simultaneous tracking of multiple GSMTC sessions |
| `int g_NumOfMedia` | ~183 | Count of currently active media sessions |
| `int g_MaxNumOfMedia = 10` | ~184 | Maximum tracked sessions cap |
| `MediaState::sourceAppId` (field) | ~165 | Source app AUMID stored per session for `BringSourceAppToFront` |
| `MediaState::isMouseOverArt` (field) | ~166 | Per-session hover state for album art |
| `MediaState::isDefaultMedia` (field) | ~167 | Flags the "default" session for `ResumeDefaultMediaIfNothingIsPlayed` |
| `bool AutoScrollTitle` setting | ~130 | Opt-in scroll (default false); baseline always scrolls |
| `bool MultipleMediaControl` setting | ~136 | Enable/disable multi-session display |
| `bool PauseOnNewMediaPlayed` setting | ~141 | Pause the current session when a new one starts |
| `bool ResumeDefaultMediaIfNothingIsPlayed` setting | ~146 | Re-enable default session when all others stop |
| `int g_artPadding = 6` | ~193 | Layout constant controlling spacing between session art tiles |
| `g_previousMedia` | ~186 | Tracks previously playing session for resume-default feature |
| `g_DefaultMediaSession` | ~187 | Stores the "default" session reference |
| `SetMediaAsDefault()` | ~520–545 | Right-click handler; marks a session as default |
| `CloseMedia()` | ~548–570 | Middle-click handler; closes (stops) a GSMTC session |
| `BringSourceAppToFront()` | ~573–630 | Double-click handler; finds and foregrounds the source app window via AUMID + exe name matching |
| `ExtractExeHint()` | ~635–650 | Parses an AUMID to extract a short exe name hint for `FindWindowByAppIdProc` |
| `FindWindowByAppIdProc()` | ~653–690 | EnumWindows callback matching by AUMID (via PKEY_AppUserModel_ID) then exe name |
| `FindBestWindowProc()` | ~693–710 | Secondary EnumWindows callback; scores windows by title/size/visibility to pick the best candidate HWND |
| `WM_LBUTTONDBLCLK` handler | ~880 | Calls `BringSourceAppToFront(g_HoverState)` |
| `WM_RBUTTONUP` handler | ~886 | Calls `SetMediaAsDefault(g_HoverState)` |
| `WM_MBUTTONUP` handler | ~892 | Calls `CloseMedia(g_HoverState)` |
| Side-by-side session rendering in `DrawMediaPanel` | ~755–840 | Iterates `g_MediaStates[0..g_NumOfMedia]`; lays out art tiles horizontally; highlights playing session with `Pen{Color::LightGray, 5}`, default session with `Pen{Color::Gray, 5}` |

### Subtractions

| Unit | Description |
|---|---|
| Single `g_MediaState` global | Replaced by `g_MediaStates[10]` array |
| `<vector>` and `<atomic>` includes | Removed (not needed by this fork's approach) |
| Always-on scroll | Controlled by new `AutoScrollTitle` setting (default false) |

### Modifications

| Unit | Change |
|---|---|
| `UpdateMediaInfo()` | Rewrites GSMTC enumeration to iterate all sessions, populate `g_MediaStates[]`, set `g_NumOfMedia`; applies PauseOnNewMediaPlayed logic |
| `SendMediaCommand(int state)` | Parameter is now an index into `g_MediaStates[]` rather than operating on the single global |
| `DrawMediaPanel()` | Rewritten to iterate sessions side-by-side; title+artist separated onto two lines (`L"\n"` separator) vs. baseline single line with `L" • "` |
| `TaskbarEventProc` / `WM_APP+10` | Updated to check `g_NumOfMedia` |
| `HideFullscreen` default | Changed to `true` (baseline: `false`) |
| `g_HoverState` | Now indexes the active session in the array (0-based) |
| `WM_MOUSEMOVE` | Maps cursor X position to a session index via art tile geometry |

---

## Attribution

| Change | Author |
|---|---|
| All changes above | Messij (declared `@author Messij` in file header) |

No secondary contributors cited in inline comments or header.

---

## Code Quality Signals

| Signal | Label | Notes |
|---|---|---|
| Multi-session support is a genuine gap in the baseline | Positive | No other approach could address this without a similar array-based refactor |
| `BringSourceAppToFront` + `FindWindowByAppIdProc` are well-structured EnumWindows patterns | Positive | Clean AUMID-first, exe-hint fallback logic |
| `PauseOnNewMediaPlayed` setting name contains a hard typo ("Mew" instead of "New") **in a predecessor version** (`hashah2311-messij`); this fork corrects it to "New" | Positive | Regression from v5 corrected here |
| `ResumeDefaultMediaIfNothingIsPlayed` logic adds non-trivial state machine for session tracking | Neutral | Increases complexity but the feature is well-defined |
| `g_MediaStates` mutex not held during `DrawMediaPanel` art draw loop (read of `albumArt` pointer) | Negative | Potential race with MediaThread updating art bitmap concurrently — same pattern as baseline but compounded by multi-session iteration |
| Side-by-side layout assumes fixed art-tile geometry; no minimum-width guard if many sessions are active | Negative | With 10 sessions and default PanelWidth=300, each tile would be ~30px — too small to be usable |
| `AutoScrollTitle` default=false diverges from baseline behavior; users who upgrade would silently lose scrolling | Neutral | Documented in settings, but a breaking default change |

---

## Synthesis Candidates from This Fork

### SC-M-1: Multi-session array and GSMTC enumeration
- **Signal:** Recommended
- **Class:** MULTI-UNIT-INTEGRATION
- **Recommended model:** Gemini 3 Pro (high), Sonnet 4.6
- **Seed:** Port `g_MediaStates[10]`, `g_NumOfMedia`, and the `UpdateMediaInfo` multi-session enumeration loop from `messij/taskbar-music-lounge-multiple/mod.wh.cpp` into a successor. Target: replace the single `g_MediaState` global. Success criterion: all existing single-session functionality continues; multiple simultaneous sessions are tracked and individually addressable. Attribute to Messij.

### SC-M-2: BringSourceAppToFront (AUMID + exe fallback window lookup)
- **Signal:** Recommended
- **Class:** TARGETED-PORT
- **Recommended model:** Gemini 3 Pro (low), Sonnet 4.5
- **Seed:** Port `BringSourceAppToFront()`, `ExtractExeHint()`, `FindWindowByAppIdProc()`, and `FindBestWindowProc()` from `messij/taskbar-music-lounge-multiple/mod.wh.cpp`. Target: double-click handler. Success criterion: double-clicking the widget foregrounds the media app, with graceful fallback for apps not found by AUMID. Attribute to Messij.

### SC-M-3: SetMediaAsDefault + CloseMedia session control
- **Signal:** Consider
- **Class:** TARGETED-PORT
- **Recommended model:** Gemini 3 Pro (low), Sonnet 4.5
- **Seed:** Port `SetMediaAsDefault()` (right-click) and `CloseMedia()` (middle-click) from `messij/taskbar-music-lounge-multiple/mod.wh.cpp`. Requires SC-M-1 (multi-session array). Target: mouse message handlers in WndProc. Success criterion: right-click marks a session as default for `ResumeDefaultMediaIfNothingIsPlayed`; middle-click stops a session. Attribute to Messij.

### SC-M-4: AutoScrollTitle opt-in scroll setting
- **Signal:** Consider
- **Class:** MECHANICAL
- **Recommended model:** Gemini 3 Flash, Claude Haiku 4.5
- **Seed:** Add `AutoScrollTitle` boolean setting from `messij/taskbar-music-lounge-multiple/mod.wh.cpp`. Target: `LoadSettings` and the scroll-advance logic in the animation timer. Success criterion: scroll only advances when setting is true; when false the text is truncated. Attribute to Messij.

---

## Flags

- **MANIFEST Duplicate filename conflict:** Row 9 in MANIFEST.md refers to the same physical file as row 3. Treated as one fork per invocation instructions. No additional attribution issues.

---

## Appendix

- Tools used: Read (offset/limit), diff-by-inspection against baseline summary
- Approximate line count: ~1567 lines
- No skips
