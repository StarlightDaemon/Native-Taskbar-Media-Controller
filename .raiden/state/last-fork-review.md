# Last Fork Review

| Field | Value |
|---|---|
| Date | 2026-05-19 |
| Baseline | `/forks/og_Hashah2311_taskbar-music-lounge.wh/og_Hashah2311_taskbar-music-lounge.wh.cpp` |
| Baseline version | v4.0.1 (Hashah2311) |
| Forks reviewed | 11 |
| Synthesis candidates | 39 |
| Critical flags | 0 |
| Synthesis report | `fork-reports/synthesis-2026-05-19.md` |
| Protocol | RAIDEN Fork Review Protocol v1 |

## Recommended Candidates (14)

| ID | Title | Fork | Class |
|---|---|---|---|
| SC-M-1 | Multi-session array and GSMTC enumeration | Messij | MULTI-UNIT-INTEGRATION |
| SC-M-2 | BringSourceAppToFront (AUMID + exe fallback) | Messij | TARGETED-PORT |
| SC-SP-1 | Seek bar with drag + TryChangePlaybackPositionAsync | memeri121 | MULTI-UNIT-INTEGRATION |
| SC-SP-2 | Seek time tooltip | memeri121 | TARGETED-PORT |
| SC-SP-3 | Layout helper functions | memeri121 | TARGETED-PORT |
| SC-SP-5 | CS_DBLCLKS window class style | memeri121 | MECHANICAL |
| SC-CH-1 | IsTaskbarEffectivelyVisible (auto-hide detection) | Chaython | TARGETED-PORT |
| SC-UI-1 | UpdateBlurredBackground (blurred album art) | Uiisland | TARGETED-PORT |
| SC-UI-2 | Adaptive text color from luminance | Uiisland | MECHANICAL |
| SC-KV-1 | RenderLayeredWindow with UpdateLayeredWindow(ULW_ALPHA) | kevinoe | MULTI-UNIT-INTEGRATION |
| SC-KV-2 | IsForegroundWindowFullscreen monitor-coverage | kevinoe | TARGETED-PORT |
| SC-HT-1 | LRC lyrics subsystem | HibritTofas | MULTI-UNIT-INTEGRATION |
| SC-HT-2 | GetAlbumPalette (64-bucket quantization) | HibritTofas | TARGETED-PORT |
| SC-GR-1 | WASAPI loopback FFT audio visualizer | GR0UD | MULTI-UNIT-INTEGRATION |

## Operator Decisions Pending

1. **Rendering pipeline:** SC-KV-1 (`UpdateLayeredWindow`) vs. retain `SetLayeredWindowAttributes` — blocks SC-UI-1 approach
2. **Multi-session support:** SC-M-1 adoption — blocks SC-M-2, SC-M-3, SC-V5-1
3. **GR0UD process compatibility audit:** `@include windhawk.exe` → `explorer.exe` — blocks SC-GR-1
4. **SC-CI-2:** ModContext architecture adoption — high-effort structural decision
5. **SC-CI-5:** Rainbow border — scope decision
6. **SC-GR-5:** Container order system — scope decision
7. **SC-UI-5:** Caps lock overlay — scope decision (out of domain for media widget)
