# Goals

## Primary Goal

Build a native XAML-injected Windhawk mod that adds a media controller to the Windows 11 taskbar — targeting `explorer.exe`, living inside the taskbar's own XAML tree (no overlay window).

## Phase Status

| Phase | Status | Branch |
|---|---|---|
| Phase 0: Fork review + synthesis | Complete | main |
| Phase 1: XAML skeleton + GSMTC | Complete | main (merged from feature/rename-to-native-controller) |
| Phase 2: Feature additions | Complete | main |
| Phase 3: Background theming | Complete → Simplified | main (Chameleon/BlurredArt shipped then removed; Acrylic retained) |
| Post-release: v1.0.x–v1.5.0 | Complete | main — latest tag v1.5.0 |

## Post-v1.4.8 Candidates

See `CURRENT_STATE.md` for the live ordered candidate list. Full synthesis at `fork-reports/synthesis-2026-05-19.md`.

**Shipped from synthesis:**
- ~~SC-SP-1: Seek bar~~ — NOT WANTED
- ✓ SC-CH-1: IsTaskbarEffectivelyVisible (Chaython)
- ✓ SC-KV-2: Monitor-coverage fullscreen detection (kevinoe)
- ✓ SC-UI-2: Adaptive text color (Uiisland)
- ✓ SC-HT-2: 64-bucket color quantization (HibritTofas) — used for Chameleon, later removed
- ✓ SC-M-2: BringSourceAppToFront (Messij)
- ✓ SC-KV-4: Track progress bar (kevinoe)
- ✓ SC-M-1: Multi-session array (Messij)
- ✓ SC-FLY-1: Hover flyout panel (Win32 WS_POPUP)
- ✓ SC-UI-1: Blurred album art background — shipped in v1.3.0, removed post-v1.4.7 (theme simplification)
- ✓ SC-M-3: Middle-click to close session

**Remaining from synthesis (not yet implemented):**
- SC-HT-1: LRC lyrics (HibritTofas)
- SC-GR-1: FFT audio visualizer (GR0UD) — requires process compatibility audit first

**Deferred / maybe later:**
- SC-HT-5: Per-app volume control — no implementation plan; deferred
- SC-HT-3: Shuffle / repeat status — low priority; deferred alongside SC-HT-5
