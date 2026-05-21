# Fork Review: Taskbar Music Lounge - v5

| Field | Value |
|---|---|
| Fork name | Taskbar Music Lounge - v5 |
| Authors | Hashah2311 / Messij |
| Source file | `/forks/hashah2311-messij/taskbar-music-lounge-v5/mod.wh.cpp` |
| Baseline file | `/forks/og_Hashah2311_taskbar-music-lounge.wh/og_Hashah2311_taskbar-music-lounge.wh.cpp` |
| Review date | 2026-05-19 |
| MANIFEST flags | Duplicate filename conflict (row 13 is same physical file as row 5 — treated as one fork) |

## Baseline Reference

See **Baseline Identification** section in `fork-reports/synthesis-2026-05-19.md` for full baseline characterization.

---

## Diff Analysis

This fork is a predecessor to `messij/taskbar-music-lounge-multiple` (v1.x). It contains the initial multi-session work but is missing the more complete v1.x features (no `SetMediaAsDefault`, no `BringSourceAppToFront`, no `ResumeDefaultMediaIfNothingIsPlayed`).

### Additions

| Unit | Lines | Description |
|---|---|---|
| `MediaState g_MediaStates[10]` | ~175 | Array of 10 MediaState structs replacing single `g_MediaState` |
| `int g_NumOfMedia` | ~178 | Count of currently active GSMTC sessions |
| `int g_MaxNumOfMedia = 10` | ~179 | Session cap |
| `bool AutoScrollTitle` setting | ~128 | Opt-in scroll (default false) |
| `bool MultipleMediaControl` setting | ~133 | Enable/disable multi-session display |
| `bool PauseOnMewMediaPlayed` setting | ~138 | **Typo: "Mew" instead of "New"**; pauses current session when a new one starts; setting key and struct field both carry this typo |
| `int g_artPadding = 6` | ~188 | Art tile spacing constant |
| `g_SelectedMedia = 0` | ~182 | Integer declared but appears unused throughout (dead code) |
| `CloseMedia()` | ~510–535 | Middle-click handler; closes (stops) a GSMTC session. Also present in v1.x. |
| `WM_RBUTTONUP` → `SendMediaCommand(g_HoverState)` | ~870 | Same as left-click; no special right-click behavior (contrast with v1.x which has SetMediaAsDefault) |
| `WM_MBUTTONUP` → `CloseMedia()` | ~876 | Middle-click to close session |
| Side-by-side session art rendering | ~740–820 | Same multi-session draw pattern as v1.x |
| Title/artist two-line layout | ~790 | `L"\n"` separator between title and artist |
| `/// ---` section delimiter comments | throughout | Extensive visual comment delimiters marking new code sections (not present in baseline or v1.x) |
| `HideFullscreen` default true | ~120 | Same as v1.x |

### Subtractions

| Unit | Description |
|---|---|
| Single `g_MediaState` global | Replaced by array |
| `SetMediaAsDefault()` | Not yet present (added in v1.x) |
| `BringSourceAppToFront()` | Not yet present (added in v1.x) |
| `ExtractExeHint()`, `FindWindowByAppIdProc()`, `FindBestWindowProc()` | Not yet present |
| `ResumeDefaultMediaIfNothingIsPlayed` setting | Not yet present |
| `sourceAppId` field in MediaState | Not yet present (added in v1.x) |

### Modifications

| Unit | Change |
|---|---|
| `UpdateMediaInfo()` | Multi-session enumeration, same pattern as v1.x |
| `SendMediaCommand(int state)` | Takes session index |
| `DrawMediaPanel()` | Multi-session side-by-side layout; two-line text |
| Version | v4.0.1 → v5.0.0 |

---

## Attribution

| Change | Author |
|---|---|
| All changes above | Hashah2311 / Messij (both declared in `@author` field; represents a collaboration between original author and fork author) |

No secondary contributors cited.

---

## Code Quality Signals

| Signal | Label | Notes |
|---|---|---|
| **`PauseOnMewMediaPlayed` typo** is a High-significance regression: the setting key is misspelled in both the settings schema and the struct; any user who configured this setting by name in v5 would have a broken setting if it were corrected to "New" | High — Negative | Corrected in v1.x (Messij's separate fork) but a clear authorship/review gap |
| `g_SelectedMedia = 0` is declared but never read or written meaningfully — dead code | Low — Negative | |
| `WM_RBUTTONUP` → same as left click is a regression vs. v1.x where right-click sets default session | Neutral | Not a regression vs. baseline (baseline has no right-click behavior); just incomplete relative to the later v1.x |
| `/// ---` comment delimiters are stylistically inconsistent with the baseline | Neutral — Low | Cosmetic |
| The core multi-session array and enumeration are functionally equivalent to v1.x | Neutral | This fork is superseded by v1.x for most purposes |

---

## Synthesis Candidates from This Fork

All multi-session additions in this fork are superseded by the more complete implementation in `messij/taskbar-music-lounge-multiple`. See SC-M-1 through SC-M-4 in that fork's report.

### SC-V5-1: CloseMedia() (middle-click session close)
- **Signal:** Consider
- **Class:** TARGETED-PORT
- **Recommended model:** Gemini 3 Pro (low), Sonnet 4.5
- **Seed:** `CloseMedia()` is also present in v1.x; prefer porting from `messij/taskbar-music-lounge-multiple/mod.wh.cpp`. Treat this fork's version as confirmation of the same design. If only this fork is used as a source, port `CloseMedia()` and `WM_MBUTTONUP` handler. Requires multi-session array (SC-M-1). Attribute to Hashah2311 / Messij.

---

## Flags

- **MANIFEST Duplicate filename conflict:** Row 13 in MANIFEST.md refers to the same physical file as row 5. Treated as one fork per invocation instructions.
- **Typo in setting key and struct field:** `PauseOnMewMediaPlayed` — corrected to `PauseOnNewMediaPlayed` in the v1.x Messij fork. Do not port the typo name.

---

## Appendix

- Tools used: Read (offset/limit), diff-by-inspection against baseline summary
- Approximate line count: ~1241 lines
- No skips
