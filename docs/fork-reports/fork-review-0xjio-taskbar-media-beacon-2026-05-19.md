# Fork Review: Taskbar Media Beacon

| Field | Value |
|---|---|
| Fork name | Taskbar Media Beacon |
| Author | 0xjio |
| Source file | `/forks/0xjio/taskbar-media-beacon/taskbar-media-beacon.wh.cpp` |
| Baseline file | `/forks/og_Hashah2311_taskbar-music-lounge.wh/og_Hashah2311_taskbar-music-lounge.wh.cpp` |
| Review date | 2026-05-19 |
| MANIFEST flags | None |

## Baseline Reference

See **Baseline Identification** section in `fork-reports/synthesis-2026-05-19.md` for full baseline characterization.

---

## Diff Analysis

This fork is a display-only widget — all playback control functionality has been removed. It retains GSMTC session tracking for reading media state but strips all `SendMediaCommand`, button rendering, and mouse action handling. The visual output is redesigned around a minimal, square album-art-centered layout.

### Additions

| Unit | Lines | Description |
|---|---|---|
| `UpdateLayeredContent()` | ~380–450 | Per-pixel alpha render path using `UpdateLayeredWindow`. Same architecture as kevinoe fork but arrived at independently. Constructs a DIB section, renders GDI+, calls `UpdateLayeredWindow` with full `BLENDFUNCTION`. |
| Two-line bold/regular text layout | ~520–570 | Title rendered in bold; artist rendered in regular weight below it. `StringTrimmingEllipsisCharacter` truncation on both lines. No scroll animation. |
| Square album art clip | ~490–520 | Album art clipped to a square region (no rounded corners on art). Layout positions art on the left; title+artist text on the right. |
| `ACCENT_DISABLED` background | ~290 | No acrylic or DWM blur; pure per-pixel alpha background via `UpdateLayeredWindow`. |
| Font `L"Inter"` | ~210 | Replaces baseline `L"Segoe UI Variable Display"`. `L"Inter"` is not a system-bundled font; falls back to a system default if not installed. |
| Window class name `WindhawkMediaBeacon_GSMTC` | ~1050 | Distinct class name from baseline's class. |
| Cursor `IDC_ARROW` set explicitly | ~1060 | No custom cursor; no pointer-change on hover. |

### Subtractions

| Unit | Description |
|---|---|
| `SendMediaCommand()` | Removed entirely |
| `DrawButtons()` / button rendering | Removed; no playback controls rendered |
| `WM_LBUTTONUP`, `WM_RBUTTONUP`, `WM_MBUTTONUP` handlers | Return 0 without action; effectively no-ops |
| Scroll animation (`IDT_ANIMATION`) | Removed |
| `SetLayeredWindowAttributes` path | Replaced by `UpdateLayeredWindow` |
| `IsSystemLightMode()`, `AutoTheme` | Removed |
| `RegisterTaskbarHook()`, taskbar event hook | Removed |
| `WM_APP+10` handler | Removed |

### Modifications

| Unit | Change |
|---|---|
| `DrawMediaPanel()` | Completely rewritten; square art clip, two-line text with ellipsis truncation, no buttons, no scroll |
| `WM_LBUTTONUP` | Present but returns 0 — no action |
| Version | v4.0.1 → v1.0.0 (renumbered by 0xjio) |

---

## Attribution

| Change | Author |
|---|---|
| All changes above | 0xjio (declared `@author 0xjio` in file header) |

No secondary contributors cited.

---

## Code Quality Signals

| Signal | Label | Notes |
|---|---|---|
| `UpdateLayeredWindow(ULW_ALPHA)` per-pixel alpha is a correct choice for a display-only widget with no button regions | Positive | Same benefit as kevinoe: true transparency at widget edges |
| Removal of all playback controls is the most coherent decision in the set — doing one thing well | Positive | Zero risk of accidental media commands; appropriate for a pure display widget |
| `StringTrimmingEllipsisCharacter` truncation is cleaner than scrolling for a "glanceable" display | Positive | No animation timer needed; no scroll state to manage |
| Font `L"Inter"` is not a system-bundled Windows font | Negative | Will silently fall back to a system serif (likely Times New Roman) on systems without Inter installed; no fallback font declaration |
| Removing the taskbar hook means the widget does not reanchor on taskbar move/restart | Negative | Same regression as memeri121 and kevinoe; consistent pattern across forks that remove the hook |
| `WM_LBUTTONUP` returning 0 without action is a subtle usability regression — users who click the widget get no feedback | Neutral | Not a defect, but a departure from the expectation set by baseline |
| Square album art clip without rounded corners diverges from baseline's rounded-rect art | Neutral | Stylistic choice; not a defect |
| The fork is the smallest in the set (~916 lines) with a very focused scope | Positive | Easy to audit; no scope creep |

---

## Synthesis Candidates from This Fork

### SC-0X-1: Display-only mode (controls-off layout)
- **Signal:** Consider
- **Class:** MECHANICAL
- **Recommended model:** Gemini 3 Flash, Claude Haiku 4.5
- **Seed:** Add a `ShowControls` boolean setting to a successor. When false, skip button rendering in `DrawMediaPanel` and return 0 from `WM_LBUTTONUP`. Source: `0xjio/taskbar-media-beacon/taskbar-media-beacon.wh.cpp`. Success criterion: widget displays title/artist/art only; no playback buttons rendered; no media command sent on click. Attribute to 0xjio.

### SC-0X-2: Two-line ellipsis text layout (no scroll)
- **Signal:** Consider
- **Class:** MECHANICAL
- **Recommended model:** Gemini 3 Flash, Claude Haiku 4.5
- **Seed:** Port the two-line bold/regular text layout with `StringTrimmingEllipsisCharacter` from `0xjio/taskbar-media-beacon/taskbar-media-beacon.wh.cpp`. Target: add as an alternative text mode in `DrawMediaPanel` (conditional on a `TextMode` setting or similar). Success criterion: title renders bold, artist renders regular weight below it, both truncate with ellipsis if too long. Attribute to 0xjio.

Note: `UpdateLayeredWindow(ULW_ALPHA)` from this fork is superseded by SC-KV-1 (kevinoe) which arrived at the same architecture independently; prefer kevinoe as the source for that pattern.

---

## Flags

- **Font fallback risk:** `L"Inter"` is not a system-bundled Windows font. Do not port the font name; use a safe system font (`L"Segoe UI Variable Display"` or similar) or add a fallback declaration.

---

## Appendix

- Tools used: Read (offset/limit), diff-by-inspection against baseline summary
- Approximate line count: ~916 lines
- No skips
