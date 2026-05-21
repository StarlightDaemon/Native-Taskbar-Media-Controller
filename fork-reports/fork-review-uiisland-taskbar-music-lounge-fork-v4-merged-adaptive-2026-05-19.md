# Fork Review: Taskbar Music Lounge - Fork (V4 Merged & Adaptive)

| Field | Value |
|---|---|
| Fork name | Taskbar Music Lounge - Fork (V4 Merged & Adaptive) |
| Author | Uiisland |
| Source file | `/forks/uiisland/taskbar-music-lounge-fork-v4-merged-adaptive/taskbar-music-lounge-fork.wh.cpp` |
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
| `UpdateBlurredBackground(HWND)` | ~480–570 | Generates a blurred-background bitmap from the current album art using a two-pass downscale: `InterpolationModeBilinear` to a small intermediate, then `InterpolationModeHighQualityBicubic` to final size. Stores result in `g_MediaState.blurredBg`. |
| `MediaState::blurredBg` (field) | ~165 | `Gdiplus::Bitmap*` storing the generated blurred background |
| `MediaState::artVersion` / `lastArtVersion` (fields) | ~166–167 | Integer version counter for dirty-detection on the blurred background; avoids regenerating when art unchanged |
| `MediaState::isDarkCover` (field) | ~168 | Boolean — true when album art's average luminance < 135 (dark cover → use white text) |
| Adaptive color extraction in `UpdateMediaInfo()` | ~640–665 | After loading album art, samples a 1×1 downscaled pixel to get average color; computes luminance; sets `isDarkCover` |
| `DrawMusicIcon()` | ~720–750 | Draws a music note (♪) icon as album art placeholder when no art is available |
| `DrawLockIcon(bool open)` | ~753–780 | Draws a padlock icon (open or closed) for caps lock notification display |
| `HWND g_hCapsWindow` | ~195 | Separate HWND for caps lock overlay |
| `CapsWndProc()` | ~810–880 | Window procedure for caps lock overlay; draws the lock icon and handles `WM_TIMER` (IDT_CAPSLOCK_DISPLAY) |
| `ShowCapsLockNotify()` | ~883–905 | Shows and positions `g_hCapsWindow` above the media widget |
| `UpdateCapsWindowAppearance()` | ~908–920 | Applies acrylic styling to the caps lock window |
| `IDT_CAPSLOCK_POLL` (200ms timer) | ~960 | Timer in the media window's WM_TIMER handler; polls `GetKeyState(VK_CAPITAL)` to detect caps lock state changes |
| `WM_UPDATE_BLUR` (`WM_APP+1`) | ~600 | Custom message that triggers `UpdateBlurredBackground` on the message thread |
| `WM_SIZE` handler | ~980 | Calls `UpdateBlurredBackground` when the window resizes |
| Mini logo mode logic | ~710–720 | When `width <= height + 10` (square-ish window), skips controls and text rendering; shows only album art or music icon |
| `BlurStrength` setting (default 8) | ~130 | Controls how much the art is downscaled for blur effect |
| `EnableMask` setting (default true) | ~133 | Enables a semi-transparent mask overlay over the blurred background |
| `MaskOpacity` setting (default 180) | ~136 | Alpha for the mask overlay |
| Font change: `L"Microsoft YaHei UI"` | ~205 | Replaces `L"Segoe UI Variable Display"` with a CJK-compatible font |
| "No media" string: `L"暂无媒体播放"` | ~712 | Chinese placeholder text instead of baseline's English equivalent |
| `ButtonScale` setting renamed to "UI Scale" in description | ~140 | Display name change only; key unchanged |
| `WM_DESTROY` cleanup of `blurredBg` | ~1050 | Deletes `g_MediaState.blurredBg` bitmap on window destruction |

### Subtractions

| Unit | Description |
|---|---|
| No settings removed from baseline | All baseline settings retained |

### Modifications

| Unit | Change |
|---|---|
| `DrawMediaPanel()` | Adds blurred background draw step at start (if `blurredBg` available); applies mask overlay on top; adaptive text color from `isDarkCover`; calls `DrawMusicIcon()` when no art; mini logo mode check |
| `UpdateMediaInfo()` | Adds adaptive color extraction after art load |
| `WM_TIMER` (IDT_POLL_MEDIA) | Adds `IDT_CAPSLOCK_POLL` check and `ShowCapsLockNotify` call |
| Target width in WM_APP+10 | When no media, collapses width to `g_Settings.height` (square mini mode) rather than full `g_Settings.width` |
| Version | v4.0.1 → v1.0.0 (renumbered by Uiisland) |

---

## Attribution

| Change | Author |
|---|---|
| All changes above | Uiisland (declared `@author Uiisland` in file header) |

Note: Fork is based on v4.0.1 of Hashah2311's baseline. Chinese-language comments and UI strings indicate Uiisland's locale/target audience. No secondary attribution.

---

## Code Quality Signals

| Signal | Label | Notes |
|---|---|---|
| Two-pass blur (bilinear → bicubic) is a good quality compromise for background art | Positive | Avoids blockiness of single-pass downscale; reasonable performance for a 48px-tall widget |
| Adaptive text color from luminance is a clean, standards-based approach (ITU-R BT.601 coefficients) | Positive | The threshold of 135 is a common default for WCAG-adjacent readability |
| `artVersion` / `lastArtVersion` dirty detection avoids regenerating blur on every frame | Positive | Correct performance guard |
| Caps lock overlay is a scope addition with no connection to the baseline's media-player purpose | Negative (Low) | Introduces a second window, a polling timer, and additional resource management for a feature entirely unrelated to media control |
| `g_hCapsWindow` creation and cleanup is not consistently guarded; `WM_DESTROY` doesn't destroy it | Negative | `g_hCapsWindow` could leak if caps lock overlay is shown when the media window closes |
| Font change to `L"Microsoft YaHei UI"` breaks non-CJK locales that don't have it installed | Negative | Windows ships this font only on Chinese language packs; will fall back to a system serif on other systems |
| Chinese "no media" string `L"暂无媒体播放"` limits usability to Chinese-language users | Neutral | Clearly intentional locale targeting; not a bug, but a barrier to general use |
| Mini logo mode (width ≤ height+10 → square display) is an elegant space-saving mode | Positive | No settings required; purely geometry-driven |
| `WM_UPDATE_BLUR` custom message triggers blur regeneration correctly from the message thread | Positive | Avoids race conditions with direct bitmap writes from WM_TIMER |

---

## Synthesis Candidates from This Fork

### SC-UI-1: UpdateBlurredBackground (blurred album art background)
- **Signal:** Recommended
- **Class:** TARGETED-PORT
- **Recommended model:** Gemini 3 Pro (low), Sonnet 4.5
- **Seed:** Port `UpdateBlurredBackground(HWND)` and `MediaState::blurredBg` / `artVersion` / `lastArtVersion` fields from `uiisland/taskbar-music-lounge-fork-v4-merged-adaptive/taskbar-music-lounge-fork.wh.cpp`. Target: draw the blurred background as the first layer in `DrawMediaPanel`, triggered by `WM_UPDATE_BLUR` (WM_APP+1) on art change. Success criterion: blurred art fills the widget background when art is available; no regeneration when art is unchanged. Attribute to Uiisland.

### SC-UI-2: Adaptive text color from album art luminance
- **Signal:** Recommended
- **Class:** MECHANICAL
- **Recommended model:** Gemini 3 Flash, Claude Haiku 4.5
- **Seed:** Port the 1×1 downscale luminance check from `UpdateMediaInfo()` in `uiisland/taskbar-music-lounge-fork-v4-merged-adaptive/taskbar-music-lounge-fork.wh.cpp`. Target: set a `isDarkCover` flag after loading album art; use it in `GetCurrentTextColor()` to return white or black text. Success criterion: text color automatically adapts to album art brightness. Attribute to Uiisland.

### SC-UI-3: DrawMusicIcon placeholder
- **Signal:** Consider
- **Class:** MECHANICAL
- **Recommended model:** Gemini 3 Flash, Claude Haiku 4.5
- **Seed:** Port `DrawMusicIcon()` from `uiisland/taskbar-music-lounge-fork-v4-merged-adaptive/taskbar-music-lounge-fork.wh.cpp`. Target: call it in `DrawMediaPanel` when `albumArt == nullptr`. Success criterion: music note icon renders in the art slot when no art is available. Attribute to Uiisland.

### SC-UI-4: Mini logo mode (width ≤ height + 10 collapses to art-only view)
- **Signal:** Consider
- **Class:** MECHANICAL
- **Recommended model:** Gemini 3 Flash, Claude Haiku 4.5
- **Seed:** Port the mini logo mode check from `DrawMediaPanel()` in `uiisland/taskbar-music-lounge-fork-v4-merged-adaptive/taskbar-music-lounge-fork.wh.cpp`. When `window_width <= window_height + 10`, skip controls and text; only render album art or music icon. Target: add as an early-out branch in `DrawMediaPanel`. Attribute to Uiisland.

### SC-UI-5: Caps lock notification overlay
- **Signal:** Flag
- **Class:** SPECULATIVE-TRIAGE
- **Recommended model:** Opus 4.7 (thinking), Sonnet 4.6 (thinking)
- **Seed:** The caps lock overlay (`g_hCapsWindow`, `CapsWndProc`, `ShowCapsLockNotify`, `IDT_CAPSLOCK_POLL`) from `uiisland/taskbar-music-lounge-fork-v4-merged-adaptive/taskbar-music-lounge-fork.wh.cpp` is a self-contained second window with no relation to media control. Operator should decide whether this belongs in a media widget successor before any porting is attempted. Known issue: `g_hCapsWindow` not destroyed in WM_DESTROY.

---

## Flags

- **Chinese-locale hardcoding:** `L"Microsoft YaHei UI"` font and `L"暂无媒体播放"` string are locale-specific. These should not be ported verbatim into a general-purpose successor.

---

## Appendix

- Tools used: Read (offset/limit), diff-by-inspection against baseline summary
- Approximate line count: ~1516 lines
- No skips
