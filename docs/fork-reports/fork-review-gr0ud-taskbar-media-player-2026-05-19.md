# Fork Review: Taskbar Media Player

| Field | Value |
|---|---|
| Fork name | Taskbar Media Player |
| Author | GR0UD |
| Source file | `/forks/gr0ud/taskbar-media-player/taskbar-media-player.wh.cpp` |
| Baseline file | `/forks/og_Hashah2311_taskbar-music-lounge.wh/og_Hashah2311_taskbar-music-lounge.wh.cpp` |
| Review date | 2026-05-19 |
| MANIFEST flags | None |

## Baseline Reference

See **Baseline Identification** section in `fork-reports/synthesis-2026-05-19.md` for full baseline characterization.

---

## Diff Analysis

**Critical architectural difference:** This mod uses `@include windhawk.exe`, not `explorer.exe`. It runs inside the Windhawk process, not the Explorer taskbar process. This is a fundamentally different deployment model from all other forks and from the baseline. Any code porting from this fork into an `@include explorer.exe` successor requires validating that the APIs and thread model are compatible in an Explorer context.

This is the second-largest fork (~3500+ lines) and introduces the most technically distinct feature in the set: a live audio visualizer using real-time FFT on WASAPI loopback audio.

### Additions

#### Process Target

| Unit | Description |
|---|---|
| `@include windhawk.exe` | Runs in Windhawk process, not Explorer. All window creation, WinRT init, and GSMTC access occur in this process context. |
| `@architecture x86-64` | Targets 64-bit only. |

#### Modular Container System

| Unit | Lines | Description |
|---|---|---|
| Container order via 4-digit code | ~210–240 | A 4-digit string setting (e.g. `"1234"`) controls which containers are shown and in what order. Each digit maps to: 1=Media, 2=Info, 3=Controls, 4=Visualizer. |
| `AnimState` struct | ~260 | Per-container animation state (slide progress, opacity) |
| `TimerSet` RAII class | ~300–340 | Manages a set of timer IDs with automatic `KillTimer` on destruction |
| `BlurCache` struct | ~350–380 | Caches the blurred album art bitmap; invalidated on art change |
| `ThemeCache` struct | ~385–420 | Caches theme-derived colors to avoid recomputing per frame |

#### Audio Visualizer (FFT)

| Unit | Lines | Description |
|---|---|---|
| `CaptureThreadProc()` | ~800–1050 | WASAPI loopback capture thread. Creates `IAudioClient` on default render endpoint with `AUDCLNT_SHAREMODE_SHARED` and `AUDCLNT_STREAMFLAGS_LOOPBACK`. Reads audio frames in ~10ms chunks. Applies Hann window function. Computes `FFT_SIZE=1024` point FFT using precomputed twiddle factors. Outputs `NUM_BANDS=7` frequency band magnitudes. Thread-safe via `std::atomic`. |
| `FFT_SIZE = 1024` | ~450 | FFT window size constant |
| `NUM_BANDS = 7` | ~451 | Number of frequency band outputs |
| 5 bar shapes | ~1800–1950 | `BarShape` enum: Rect, RoundTop, Circle, Diamond, Wave |
| 5 color modes | ~1960–2100 | `ColorMode` enum: Solid, Gradient, Spectrum, Reactive, Album |
| 6 EQ presets | ~2110–2200 | `EQPreset` enum: Flat, Bass, Treble, Mid, VShape, Custom; applies frequency-domain gain before band extraction |
| Twiddle factor precomputation | ~700–760 | Complex exponentials `e^{-2πik/N}` precomputed at startup; stored as float pairs for FFT inner loop performance |
| Hann window application | ~770–800 | Multiplies time-domain samples by `0.5 * (1 - cos(2π*n/(N-1)))` before FFT |

#### Text

| Unit | Lines | Description |
|---|---|---|
| `TextCrossfade` struct | ~430–460 | Manages opacity-based crossfade between old and new text when track changes. Two strings (current/previous), transition progress float. |

#### Theming

| Unit | Lines | Description |
|---|---|---|
| 12 background themes | ~180–210 | `BgTheme` enum with 12 values including: Transparent, Acrylic, Blur, Solid, Gradient, ArtBlur, ArtGradient, ArtSolid, Rainbow, AudioReactive, and others |

#### Error Handling Macros

| Unit | Lines | Description |
|---|---|---|
| `WH_CATCH` / `WH_TRY_OR` macros | ~100–120 | Exception-catching macros used throughout; `WH_TRY_OR(fallback)` evaluates to `fallback` on exception. Prevents crashes from WinRT/COM exceptions propagating to Windhawk's message loop. |

#### Settings

| Unit | Description |
|---|---|
| Container order code | 4-digit string |
| Visualizer settings | BarShape, ColorMode, EQPreset, BarCount, BarSpacing, BarWidth, SensitivityMultiplier, PeakDecay |
| Theme settings | BgTheme, AccentColor, ArtBlurStrength |
| All baseline settings | Retained or equivalent |

#### System

| Unit | Lines | Description |
|---|---|---|
| Per-app volume | ~1200–1300 | Same `IAudioSessionManager2` → `ISimpleAudioVolume` pattern as HibritTofas |
| `g_TbCreatedMsg` | ~245 | Registers for `TaskbarCreated` window message (same as baseline) |
| `MediaCmd` enum | ~250 | Typed enum replacing raw integer in `SendMediaCommand` |

### Subtractions

| Unit | Description |
|---|---|
| Flat globals | All replaced by organized structs |
| Inline coordinate math in DrawMediaPanel | Replaced by container layout system |

### Modifications

| Unit | Change |
|---|---|
| `UpdateMediaInfo()` | Triggers `BlurCache` invalidation on art change; populates additional state for visualizer |
| `DrawMediaPanel()` | Container-based layout; iterates active containers in configured order |
| `SendMediaCommand()` | Takes `MediaCmd` enum instead of raw int |
| Version | v4.0.1 → v6.3.0 (major version divergence) |

---

## Attribution

| Change | Author |
|---|---|
| All changes above | GR0UD (declared `@author GR0UD` in file header) |

No secondary contributors cited.

---

## Code Quality Signals

| Signal | Label | Notes |
|---|---|---|
| WASAPI loopback FFT with Hann window and twiddle factor precomputation is the highest-quality audio visualization implementation in the set | Positive | Twiddle precomputation is the correct optimization for a real-time path; Hann window suppresses spectral leakage |
| `WH_CATCH`/`WH_TRY_OR` macros for WinRT exception containment are a defensive best practice | Positive | WinRT async operations throw on failure; catching at call sites prevents widget crashes |
| `TimerSet` RAII for timer lifecycle is a clean pattern that eliminates KillTimer leaks | Positive | Correct application of RAII in a WndProc context |
| `@include windhawk.exe` is a critical incompatibility with all other forks | Negative (High) | Code runs in a different process. GDI window handles, thread apartment state, and WinRT init are all process-scoped. Any port to an `@include explorer.exe` successor requires full compatibility audit. |
| Container order via a 4-digit code is an unusual but flexible UX design | Neutral | Power-user ergonomics; no discoverability without documentation |
| 12 background themes adds settings complexity; most themes overlap in capability with Uiisland and HibritTofas candidates | Neutral | Not a defect; the themes appear well-defined |
| `TextCrossfade` struct is a clean approach to track-change animation | Positive | Avoids flash/blank when text updates |
| 5 bar shapes and 5 color modes for the visualizer add significant settings surface with low implementation risk (pure rendering) | Neutral | Reasonable for a visualizer-first fork |

---

## Synthesis Candidates from This Fork

### SC-GR-1: WASAPI loopback FFT audio visualizer
- **Signal:** Recommended
- **Class:** MULTI-UNIT-INTEGRATION
- **Recommended model:** Gemini 3 Pro (high), Sonnet 4.6
- **Seed:** Port `CaptureThreadProc()`, twiddle factor precomputation, Hann window application, FFT_SIZE=1024, and NUM_BANDS=7 band extraction from `gr0ud/taskbar-media-player/taskbar-media-player.wh.cpp`. Target: add a `Visualizer` container to a successor that renders 7 frequency band bars. **Critical constraint:** this code runs in `@include windhawk.exe` — before porting, verify that `IAudioClient` loopback capture, `std::atomic`, and the capture thread are safe to initialize in an `@include explorer.exe` process. If incompatible, the WASAPI initialization path must be adjusted for Explorer's COM apartment model. Success criterion: 7 frequency band bars animate in real time during audio playback. Attribute to GR0UD.

### SC-GR-2: TextCrossfade struct
- **Signal:** Consider
- **Class:** MECHANICAL
- **Recommended model:** Gemini 3 Flash, Claude Haiku 4.5
- **Seed:** Port `TextCrossfade` struct from `gr0ud/taskbar-media-player/taskbar-media-player.wh.cpp`. Target: use in `DrawMediaPanel` to cross-fade old and new title/artist strings when track changes. Requires an alpha interpolation step in the animation timer. Success criterion: text fades out/in on track change rather than changing abruptly. Attribute to GR0UD.

### SC-GR-3: WH_CATCH / WH_TRY_OR exception macros
- **Signal:** Consider
- **Class:** MECHANICAL
- **Recommended model:** Gemini 3 Flash, Claude Haiku 4.5
- **Seed:** Port `WH_CATCH` and `WH_TRY_OR` macros from `gr0ud/taskbar-media-player/taskbar-media-player.wh.cpp`. Target: wrap all WinRT/COM call sites in `UpdateMediaInfo()` and async callbacks. Success criterion: a WinRT exception (e.g., session revoked mid-call) is caught and logged rather than propagating to the Windhawk/Explorer message loop. Attribute to GR0UD.

### SC-GR-4: MediaCmd enum (typed SendMediaCommand parameter)
- **Signal:** Consider
- **Class:** MECHANICAL
- **Recommended model:** Gemini 3 Flash, Claude Haiku 4.5
- **Seed:** Replace the raw int parameter in `SendMediaCommand(int)` with a `MediaCmd` enum from `gr0ud/taskbar-media-player/taskbar-media-player.wh.cpp`. Target: all call sites in WndProc mouse handlers. Success criterion: compiler catches invalid command values; no behavioral change. Attribute to GR0UD.

### SC-GR-5: Container order system (4-digit layout code)
- **Signal:** Flag
- **Class:** SPECULATIVE-TRIAGE
- **Recommended model:** Opus 4.7 (thinking), Sonnet 4.6 (thinking)
- **Seed:** The container order system from `gr0ud/taskbar-media-player/taskbar-media-player.wh.cpp` allows users to configure which containers (Media, Info, Controls, Visualizer) appear and in what order via a 4-digit code. Operator should decide whether a successor wants user-configurable container layout before porting. If yes, the `AnimState` struct and container iteration loop should be ported together.

---

## Flags

- **Process target incompatibility:** `@include windhawk.exe` — all code in this fork runs in the Windhawk process, not Explorer. Any port to an `@include explorer.exe` successor must audit COM apartment compatibility, thread model, and GSMTC initialization order before assuming the code will work.

---

## Appendix

- Tools used: Read (offset/limit in chunks of 600 lines due to file size), diff-by-inspection against baseline summary
- Approximate line count: ~3500+ lines
- No skips
