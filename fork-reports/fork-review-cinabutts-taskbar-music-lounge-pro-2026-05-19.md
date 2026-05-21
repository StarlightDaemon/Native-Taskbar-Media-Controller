# Fork Review: Taskbar Music Lounge Pro

| Field | Value |
|---|---|
| Fork name | Taskbar Music Lounge Pro |
| Author | Cinabutts |
| Source file | `/forks/cinabutts/taskbar-music-lounge-pro/taskbar-music-lounge-pro.wh.cpp` |
| Baseline file | `/forks/og_Hashah2311_taskbar-music-lounge.wh/og_Hashah2311_taskbar-music-lounge.wh.cpp` |
| Review date | 2026-05-19 |
| MANIFEST flags | None |

## Baseline Reference

See **Baseline Identification** section in `fork-reports/synthesis-2026-05-19.md` for full baseline characterization.

---

## Diff Analysis

This fork is the largest in the set (~6000+ lines, 292.9KB). It is a comprehensive architectural rework built on top of the baseline, replacing the flat globals structure with a deeply nested `ModContext` struct, introducing a `WindowManager` class, an action engine, a palette engine, audio peak metering, and a rainbow border effect subsystem.

### Additions

#### Architectural Additions

| Unit | Lines | Description |
|---|---|---|
| `ModContext g_Ctx` | ~200–450 | Central state struct with deeply nested sub-structs: `Wnd` (window handles, timers), `Sys` (taskbar state, monitors), `Vis` (visual settings cache), `Input` (mouse state, hover), `Rainbow` (rainbow border state, HSV), `Audio` (peak meter COM pointers), `Text` (scroll state), `Media` (media info), `ArtCache` (cached bitmaps), `Persisted` (saved position), `PaletteEngine` (15 palette slots) |
| `WindowManager` class | ~460–650 | Encapsulates window creation, message dispatch, and `ExecuteCachedBufferedPaint()` — a double-buffered paint method that only repaints when content is dirty |
| `EffectiveW()`, `EffectiveH()`, `EffectiveX()`, `EffectiveY()` | ~700–720 | Accessors returning computed layout dimensions accounting for rainbow border padding and visibility state |

#### Audio

| Unit | Lines | Description |
|---|---|---|
| `AudioCOMAPI` struct | ~750–850 | Manages `IAudioMeterInformation` COM interface for reading audio peak levels. Initializes via `IMMDeviceEnumerator` → default endpoint → `IAudioMeterInformation`. Polls peak in `Update()`. |
| Audio reactive rainbow (7 modes) | ~2800–3100 | Rainbow border color and intensity can be driven by the audio peak level in 7 distinct reactive modes |

#### Visual Effects

| Unit | Lines | Description |
|---|---|---|
| Rainbow border effect | ~1800–2200 | Separate child HWND for the border; HSV color cycle; configurable speed, saturation, brightness, width. Animated independently of the media panel. |
| `TransitionToTargetColor()` | ~3200–3280 | Smooth color interpolation toward a target color over time |
| `SnapshotPalette()` | ~3290–3340 | Captures the current palette state to a slot |
| `SmoothPalette()` | ~3350–3420 | Applies smooth interpolation between palette slots |
| `SamplePaletteSmooth()` | ~3430–3500 | Samples the active palette with smooth blending |
| Slide animations | ~2400–2550 | Width and position slide-in/out animations driven by timer |
| Smart docking | ~1200–1260 | `DOCK_PEEK_PIXELS=2` constant; logic to peek the widget at 2px when auto-hidden; full show on hover |

#### Palette Engine

| Unit | Lines | Description |
|---|---|---|
| `PaletteStorage` namespace | ~3550–3650 | Manages 15 named palette slots; CSV hex encoding for `Wh_SetStringValue` persistence |
| Palette slot R/W | ~3660–3750 | `SavePalette()` / `LoadPalette()` using `Wh_SetStringValue` / `Wh_GetStringValue` |
| `ColorTheme` enum | ~180 | `System`, `Custom`, `Artwork` — three theming modes |

#### Action Engine

| Unit | Lines | Description |
|---|---|---|
| Action engine (25+ actions) | ~4000–5200 | Configurable input triggers (single-click, double-click, right-click, middle-click, scroll-up, scroll-down, hover-enter, hover-leave) mapped to 25+ named actions including: PlayPause, Next, Prev, VolumeUp, VolumeDown, Mute, OpenApp, CloseApp, SetDefault, ResumeDefault, ShowLyrics (stub), ToggleCompact, TogglePalette, SnapshotPalette, CycleTheme, ToggleRainbow, ToggleAudioReactive, and more |

#### Registry / System

| Unit | Lines | Description |
|---|---|---|
| `RegistryManager` | ~850–980 | Monitors HKCU auto-hide registry keys on a background listener thread; fires a callback when taskbar auto-hide state changes |
| Persistent position | ~1050–1100 | `SaveUIState()` / `LoadUIState()` via `Wh_SetStringValue` / `Wh_GetIntValue`; saves and restores widget X/Y position across sessions |

#### Settings

| Unit | Description |
|---|---|
| All baseline settings retained | `PanelWidth`, `PanelHeight`, `FontSize`, `ButtonScale`, `HideFullscreen`, `IdleTimeout`, `OffsetX`, `OffsetY`, `AutoTheme`, `TextColor`, `BgOpacity` |
| Rainbow settings (7+) | Enable, Speed, Saturation, Brightness, Width, ReactiveMode, AudioSource |
| Palette settings (15 slots) | Stored as CSV hex; each slot has a name |
| Action bindings (8+ trigger → action mappings) | All configurable |
| `ColorTheme` setting | System/Custom/Artwork |
| Smart docking settings | Enable, PeekPixels |
| Slide animation settings | Enable, Duration |

### Subtractions

| Unit | Description |
|---|---|
| Flat globals | All replaced by `ModContext g_Ctx` sub-structs |

### Modifications

| Unit | Change |
|---|---|
| `WndProc` | Delegates all message handling through `WindowManager`; action engine intercepts mouse messages |
| `DrawMediaPanel` | Calls `ExecuteCachedBufferedPaint` pattern; renders through `ColorTheme` abstraction |
| `UpdateMediaInfo` | Populates `g_Ctx.Media` sub-struct; triggers palette sampling when `Artwork` theme is active |
| `@include` | `explorer.exe` (same as baseline) |

---

## Attribution

| Change | Author |
|---|---|
| All changes above | Cinabutts (declared `@author Cinabutts` in file header) |

No secondary contributors cited.

---

## Code Quality Signals

| Signal | Label | Notes |
|---|---|---|
| `ModContext` nested struct is a clean encapsulation of all global state — a genuine architectural improvement for a codebase of this scale | Positive | Eliminates ~30+ scattered globals; easier to reason about lifetime and dependencies |
| `WindowManager::ExecuteCachedBufferedPaint` avoids redundant repaints — correct optimization | Positive | Dirty-flagging pattern is appropriate for a widget that updates infrequently |
| `AudioCOMAPI` peak metering is well-isolated and properly initializes/releases COM | Positive | Clean RAII-style struct; no leaks in the reviewed path |
| `RegistryManager` auto-hide listener thread is the correct approach for detecting taskbar auto-hide changes without polling | Positive | Uses `RegNotifyChangeKeyValue` (implied by background thread pattern); avoids 1000ms polling lag |
| Action engine with 25+ configurable actions at 8 trigger points is over-engineered for a taskbar widget | Negative (High) | Adds ~1200 lines of indirection for functionality achievable with a 50-line switch in WndProc; increases maintenance surface significantly |
| Palette Engine with 15 saved slots, CSV encoding, and smooth interpolation has no clear use case in a media widget | Negative (Medium) | Palette persistence and slot management add ~200 lines for a feature whose UX value is unclear |
| Rainbow border as a separate HWND is technically sound but introduces a second window lifecycle to manage | Neutral | The implementation appears correct; the feature is optional via settings |
| `ColorTheme::Artwork` mode could conflict with `blurredBg` from Uiisland fork if both are synthesized | Neutral | Not a defect in this fork; a synthesis interaction to note |
| 6000+ lines is the largest file in the set; chunk-reading was required due to 256KB read limit | Neutral | Size alone is not a defect, but increases integration risk |
| `stringtools` namespace with string utilities is a useful addition | Positive | Avoids reinventing string conversion in WndProc paths |

---

## Synthesis Candidates from This Fork

### SC-CI-1: AudioCOMAPI peak metering (IAudioMeterInformation)
- **Signal:** Recommended
- **Class:** TARGETED-PORT
- **Recommended model:** Gemini 3 Pro (low), Sonnet 4.5
- **Seed:** Port the `AudioCOMAPI` struct from `cinabutts/taskbar-music-lounge-pro/taskbar-music-lounge-pro.wh.cpp`. It provides `IAudioMeterInformation` peak level access for the default audio endpoint. Target: make peak level available as a float (0.0–1.0) for optional audio-reactive visual effects. Do NOT port the 7-mode audio-reactive rainbow — port only the COM initialization, `Update()` poll, and `GetPeak()` accessor. Success criterion: `AudioCOMAPI::GetPeak()` returns a valid float during audio playback; COM initialized and released correctly in `ModInit`/`ModUninit`. Attribute to Cinabutts.

### SC-CI-2: ModContext nested struct pattern
- **Signal:** Consider
- **Class:** SPECULATIVE-TRIAGE
- **Recommended model:** Opus 4.7 (thinking), Sonnet 4.6 (thinking)
- **Seed:** The `ModContext g_Ctx` pattern from `cinabutts/taskbar-music-lounge-pro/taskbar-music-lounge-pro.wh.cpp` replaces scattered globals with a single nested struct. Operator should decide whether a successor should adopt this architecture before any synthesis proceeds. The pattern is sound but requires a full rewrite of all global accesses — a high-effort, low-risk-return trade unless the successor is already planned as a ground-up rewrite.

### SC-CI-3: RegistryManager auto-hide listener
- **Signal:** Consider
- **Class:** TARGETED-PORT
- **Recommended model:** Gemini 3 Pro (low), Sonnet 4.5
- **Seed:** Port the `RegistryManager` background-thread registry watcher from `cinabutts/taskbar-music-lounge-pro/taskbar-music-lounge-pro.wh.cpp`. Target: detect taskbar auto-hide state changes without polling. Success criterion: a callback fires within ~100ms of the auto-hide registry key changing. Attribute to Cinabutts.

### SC-CI-4: Persistent position (SaveUIState / LoadUIState)
- **Signal:** Consider
- **Class:** MECHANICAL
- **Recommended model:** Gemini 3 Flash, Claude Haiku 4.5
- **Seed:** Port `SaveUIState()` and `LoadUIState()` from `cinabutts/taskbar-music-lounge-pro/taskbar-music-lounge-pro.wh.cpp`. Uses `Wh_SetStringValue`/`Wh_GetIntValue` to persist widget X/Y position across sessions. Target: call `LoadUIState` in `WM_CREATE` and `SaveUIState` in `WM_DESTROY` (or on position change). Success criterion: widget remembers its last position after mod restart. Attribute to Cinabutts.

### SC-CI-5: Rainbow border effect
- **Signal:** Flag
- **Class:** SPECULATIVE-TRIAGE
- **Recommended model:** Opus 4.7 (thinking), Sonnet 4.6 (thinking)
- **Seed:** The rainbow border (`g_Ctx.Rainbow`, separate HWND, HSV cycle, audio-reactive modes) from `cinabutts/taskbar-music-lounge-pro/taskbar-music-lounge-pro.wh.cpp` is a self-contained visual feature with no connection to media control. Operator should decide whether this belongs in a media widget successor before porting is attempted. The implementation requires managing a second child window lifecycle alongside the main panel.

---

## Flags

None.

---

## Appendix

- Tools used: Read (offset/limit in chunks of 700 lines due to 256KB file size), diff-by-inspection against baseline summary
- Approximate line count: ~6000+ lines (292.9KB)
- No skips; file read in full across multiple offset windows
