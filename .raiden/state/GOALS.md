# Goals

## Primary Goal

Build a native XAML-injected Windhawk mod that adds a media controller to the Windows 11 taskbar — targeting `explorer.exe`, living inside the taskbar's own XAML tree (no overlay window).

## Phase Status

| Phase | Status | Branch |
|---|---|---|
| Phase 0: Fork review + synthesis | Complete | main |
| Phase 1: XAML skeleton + GSMTC | Complete | feature/rename-to-native-controller |
| Phase 2: Feature additions | Not started | — |

## Phase 2 Candidates (from synthesis-2026-05-19.md)

Operator decision required on prioritization. Top candidates by synthesis rating:

**Recommended (MULTI-UNIT-INTEGRATION):**
- SC-SP-1: Seek bar (memeri121)
- SC-HT-1: LRC lyrics (HibritTofas)
- SC-GR-1: FFT audio visualizer (GR0UD) — requires process compatibility audit first

**Recommended (TARGETED-PORT):**
- SC-CH-1: IsTaskbarEffectivelyVisible auto-hide fix (Chaython)
- SC-KV-2: Monitor-coverage fullscreen detection (kevinoe)
- SC-UI-1: Blurred album art background (Uiisland) — rendering pipeline conflict with ULW
- SC-HT-2: GetAlbumPalette color quantization (HibritTofas)
- SC-M-2: BringSourceAppToFront (Messij)

**Mechanical (low complexity):**
- SC-UI-2: Adaptive text color from luminance (Uiisland)
- SC-SP-5: CS_DBLCLKS window class style — N/A (no Win32 window in this architecture)
- SC-KV-5: UpdateOneSession returning bool changed
