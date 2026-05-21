# Fork Review: Taskbar Desktop Indicator

| Field | Value |
|---|---|
| Fork name | Taskbar Desktop Indicator |
| Author | Simon Benedict |
| Source file | `/forks/simon-benedict/taskbar-desktop-indicator/taskbar-desktop-indicator.txt` |
| Baseline file | `/forks/og_Hashah2311_taskbar-music-lounge.wh/og_Hashah2311_taskbar-music-lounge.wh.cpp` |
| Review date | 2026-05-19 |
| MANIFEST flags | None |

## Baseline Reference

See **Baseline Identification** section in `fork-reports/synthesis-2026-05-19.md` for full baseline characterization.

---

## Diff Analysis

**This file is not a fork of the baseline.** It is a completely different Windhawk mod that modifies the Windows taskbar clock to display a virtual desktop indicator. It shares no code, no data structures, no GSMTC usage, and no rendering architecture with the baseline. The only shared characteristics are: the Windhawk mod format, the `@include explorer.exe` target, and the use of Windows APIs.

The file uses a `.txt` extension rather than `.wh.cpp`, which is unusual and may indicate it was authored outside the standard Windhawk toolchain or submitted as a text snippet rather than a compiled mod.

### Summary of this mod's design

| Area | Description |
|---|---|
| Purpose | Injects a virtual desktop number/name indicator into the Explorer taskbar clock TextBlock |
| Architecture | XAML hook into the clock area; no separate window created |
| `@include` | `explorer.exe` |
| `@architecture` | `x86-64` |
| Entry point | Hooks `ClockEntry` XAML elements (TextBlock, Grid, StackPanel) via weak references |
| Desktop tracking | Uses `IVirtualDesktopNotificationService` COM interface to receive `VirtualDesktopChanged` callbacks |
| Display modes | Number (e.g. "1"), Markers (dots), DesktopName (user-defined name string) |
| Version compatibility | Configures `IVirtualDesktopNotificationService` variant based on explorer build number (multiple COM interface versions across Windows 11 builds) |
| Settings | `DisplayMode` (Number/Markers/DesktopName), `TextColor`, `FontSize`, `Position` (Above/Below/Replace clock), custom name list |
| Rendering | GDI-free; pure XAML property manipulation (`FontSize`, `Foreground`, `Text`) |
| Timers | None |
| Version | v1.3 |

### What this mod does NOT share with the baseline

- No `MediaState` or GSMTC
- No album art
- No GDI+ rendering
- No `WS_EX_LAYERED` or separate panel window
- No taskbar-position hook (it IS inside the taskbar, not overlaid on it)
- No playback controls

---

## Attribution

| Change | Author |
|---|---|
| Entire file | Simon Benedict (declared `@author Simon Benedict` in file header) |

No secondary contributors cited.

---

## Code Quality Signals

| Signal | Label | Notes |
|---|---|---|
| `IVirtualDesktopNotificationService` version branching by build number is the correct approach for cross-build compatibility | Positive | Windows 11 changed the virtual desktop COM interface at least twice; the version detection is necessary |
| XAML weak references for `ClockEntry` are the appropriate way to hook Explorer XAML without holding strong refs | Positive | Avoids keeping Explorer XAML objects alive after they are destroyed |
| Pure XAML property manipulation (no GDI+ or separate HWND) is the minimal-footprint approach for text injection | Positive | No rendering pipeline to maintain |
| `.txt` extension is non-standard for a `.wh.cpp` mod | Neutral | Does not affect functionality; may indicate manual submission |
| This mod solves a completely different problem than all other forks | Neutral | Not a quality signal, but a fundamental scope difference |

---

## Synthesis Candidates from This Fork

No synthesis candidates. This mod is entirely outside the domain of the baseline (media widget / GSMTC). No code or pattern from this file is applicable to a media widget successor.

The `IVirtualDesktopNotificationService` pattern may be relevant if a future successor wishes to add virtual desktop awareness (e.g., hide the media widget on specific desktops), but that is a speculative scope extension beyond any current requirement.

---

## Flags

- **Domain mismatch:** This file is not a fork of the baseline. It is an independent mod for a different feature (virtual desktop indicator). No code is portable to a media widget successor without scope expansion.
- **`.txt` extension:** File uses `.txt` instead of `.wh.cpp`. Content is valid C++ Windhawk mod format. No functional issue.

---

## Appendix

- Tools used: Read (offset/limit), structural inspection
- Approximate line count: ~1500 lines
- No skips
