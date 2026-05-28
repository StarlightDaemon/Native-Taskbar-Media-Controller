# Fork Review: Taskbar Music Lounge (v4.0.3)

| Field | Value |
|---|---|
| Fork name | Taskbar Music Lounge |
| Authors | Hashah2311 & Chaython |
| Source file | `/forks/hashah2311-chaython/taskbar-music-lounge/taskbar-music-lounge.wh.cpp` |
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
| `IsTaskbarEffectivelyVisible(HWND hTaskbar)` | 525–547 | New function. Uses `GetWindowRect` + `MonitorFromWindow` + `GetMonitorInfo` + `IntersectRect` to compute how much of the taskbar is visible on its monitor. Returns false if the visible intersection has ≤30px width or ≤30px height. Correctly detects auto-hide taskbar in its fully-slid-away state without relying on `IsWindowVisible` alone. |

The complete addition:
```cpp
bool IsTaskbarEffectivelyVisible(HWND hTaskbar) {
    if (!hTaskbar || !IsWindowVisible(hTaskbar)) return false;
    RECT rc; GetWindowRect(hTaskbar, &rc);
    HMONITOR hMon = MonitorFromWindow(hTaskbar, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (GetMonitorInfo(hMon, &mi)) {
        RECT intersect;
        if (IntersectRect(&intersect, &rc, &mi.rcMonitor)) {
            int visibleWidth  = intersect.right  - intersect.left;
            int visibleHeight = intersect.bottom - intersect.top;
            if (visibleWidth <= 30 || visibleHeight <= 30) return false;
        } else { return false; }
    }
    return true;
}
```

### Subtractions

| Unit | Description |
|---|---|
| `#include <vector>` | Removed (not used in this fork) |
| `#include <atomic>` | Removed (not used in this fork) |
| `if (!IsWindowVisible(hTaskbar))` guard in WM_APP+10 handler | Replaced by `IsTaskbarEffectivelyVisible` call |
| TaskbarVisible restore guard in WM_TIMER | Simplified (see Modifications) |

### Modifications

| Unit | Change |
|---|---|
| `WM_TIMER` handler — IDT_POLL_MEDIA | Adds check: after evaluating whether to hide due to fullscreen/idle, calls `IsTaskbarEffectivelyVisible(hTaskbar)` to force-hide the widget when the taskbar has slid off-screen (auto-hide state). Baseline only checked `IsWindowVisible`. |
| `WM_APP+10` handler (taskbar moved/appeared) | Replaces `if (!IsWindowVisible(hTaskbar))` with `if (!IsTaskbarEffectivelyVisible(hTaskbar))`; the widget stays hidden while the taskbar is auto-hidden rather than snapping into view on any taskbar position change event. |
| WM_TIMER restore path | Simplified: removes the `TaskbarVisible` intermediate variable; just calls `ShowWindow(hwnd, SW_SHOWNOACTIVATE)` directly when conditions allow. No behavioral difference in non-auto-hide scenarios. |
| Version bump | v4.0.1 → v4.0.3 |

---

## Attribution

| Change | Author |
|---|---|
| `IsTaskbarEffectivelyVisible()` and integration | Chaython (credited as co-author in `@author Hashah2311 & Chaython`; Hashah2311 is baseline author; this function is the only substantive addition, strongly attributing authorship of the change to Chaython) |

Note: the file header lists both authors. Hashah2311 owns the baseline; Chaython is the contributing co-author for the changes in this fork. Attribution is split per the file declaration; no inline secondary attribution is present.

---

## Code Quality Signals

| Signal | Label | Notes |
|---|---|---|
| `IsTaskbarEffectivelyVisible` is a genuine bug fix for auto-hide taskbar detection | Positive | Baseline's `IsWindowVisible` returns true even when the taskbar is in its 1–2px peeking state during auto-hide, causing the widget to remain visible above a hidden taskbar |
| Uses `GetMonitorInfo` + `IntersectRect` for a DPI-safe geometry check | Positive | More robust than checking window style flags or comparing pixel coordinates against a hardcoded threshold |
| 30px threshold is reasonable but arbitrary | Neutral | A very narrow taskbar (e.g. user has set 1px autohide strip) could produce a false negative; no comment explains the constant |
| Removing `<vector>` and `<atomic>` includes is correct (neither is used in this fork) | Positive | Reduces compilation dependencies |
| Simplified WM_TIMER restore path eliminates an unnecessary local variable | Positive | Minor clarity improvement |
| Fork is conservative — only the auto-hide fix is added; no feature scope creep | Positive | Easy to audit and merge |

---

## Synthesis Candidates from This Fork

### SC-CH-1: IsTaskbarEffectivelyVisible (auto-hide detection)
- **Signal:** Recommended
- **Class:** TARGETED-PORT
- **Recommended model:** Gemini 3 Pro (low), Sonnet 4.5
- **Seed:** Port `IsTaskbarEffectivelyVisible(HWND hTaskbar)` from `hashah2311-chaython/taskbar-music-lounge/taskbar-music-lounge.wh.cpp` (lines 525–547) into a successor, replacing the bare `IsWindowVisible(hTaskbar)` guard in both the WM_TIMER polling path and the WM_APP+10 taskbar-reposition handler. Success criterion: widget hides correctly when taskbar auto-hides to ≤30px; widget reappears when taskbar slides back out. Attribute to Hashah2311 & Chaython.

---

## Flags

None.

---

## Appendix

- Tools used: Read (offset/limit), diff-by-inspection against baseline summary
- Approximate line count: ~1034 lines
- No skips
