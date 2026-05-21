# Fork Review Log

---

## 2026-05-19 — Taskbar Music Lounge Fork Review

| Field | Value |
|---|---|
| Date | 2026-05-19 |
| Baseline | `/forks/og_Hashah2311_taskbar-music-lounge.wh/og_Hashah2311_taskbar-music-lounge.wh.cpp` |
| Baseline version | v4.0.1 |
| Forks reviewed | 11 (from 13 MANIFEST rows; 2 duplicate-filename-conflict flags) |
| Synthesis candidates | 39 (14 Recommended, 21 Consider, 4 Flag) |
| Critical flags | 0 |
| Secrets detected | 0 |
| Reports written | 11 per-fork reports + 1 synthesis report |
| Output directory | `fork-reports/` |
| Synthesis report | `fork-reports/synthesis-2026-05-19.md` |

### Forks Reviewed

| Fork | Author | Lines |
|---|---|---|
| taskbar-music-lounge-multiple | Messij | ~1567 |
| taskbar-spotify-widget | memeri121 | ~1603 |
| taskbar-music-lounge (co-author Chaython) | Hashah2311 & Chaython | ~1034 |
| taskbar-music-lounge-fork-v4-merged-adaptive | Uiisland | ~1516 |
| taskbar-music-lounge-v5 | Hashah2311 / Messij | ~1241 |
| taskbar-media-widget | kevinoe | ~1230 |
| taskbar-music-lounge-pro | Cinabutts | ~6000+ |
| taskbar-media-beacon | 0xjio | ~916 |
| taskbar-desktop-indicator | Simon Benedict | ~1500 |
| taskbar-media-bar | HibritTofas | ~2500+ |
| taskbar-media-player | GR0UD | ~3500+ |

### Key Findings

- GR0UD fork uses `@include windhawk.exe` — process incompatibility with all other forks; audit required before porting FFT visualizer
- Simon Benedict fork is a different mod (virtual desktop indicator); no synthesis candidates
- Three forks (memeri121, kevinoe, 0xjio) independently removed the taskbar hook — regression in all three
- Two convergence clusters: `UpdateLayeredWindow` (kevinoe + 0xjio); `BringSourceAppToFront` AUMID pattern (Messij + HibritTofas)
- Hashah-Messij v5 has `PauseOnMewMediaPlayed` typo (both key and struct); corrected in Messij v1.x
- Unique high-value features: seek bar (memeri121), LRC lyrics (HibritTofas), FFT visualizer (GR0UD)
