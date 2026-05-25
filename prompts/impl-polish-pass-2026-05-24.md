# Handoff: Polish Pass — SC-GR-2 + SC-SP-4 + SC-HT-4

**File to edit:** `native-taskbar-media-controller.wh.cpp`  
**Do not create new files. Do not change any other files.**

---

## Context

This is a Windhawk mod that injects a native XAML widget into the Windows 11 taskbar
(`explorer.exe`). The widget lives inside the taskbar's own XAML island — no overlay
window, no GDI+, no separate HWND. All UI runs on the XAML dispatcher.

The three features in this pass are pure polish — no new data pipelines, no new settings
required (though each has an optional gating setting noted below). Each is independent of
the others; implement in any order.

---

## What Already Exists (Do Not Duplicate)

- `winrt/Windows.UI.Xaml.Media.Animation.h` is already included (line 145)
- `Storyboard`, `DoubleAnimationUsingKeyFrames`, `DiscreteObjectKeyFrame`,
  `EasingDoubleKeyFrame`, `TranslateTransform`, `MakeTimeSpan()` are all in active use by
  the marquee scroll feature
- `g_MarqueeStoryboard` (line 299) and `StopMarquee()` (line 981) show the global
  storyboard pattern to follow
- `g_WidgetRoot` (weak_ref<Grid>, line 290) + `g_WidgetMutex` (line 289) is how you
  access the widget from a timer or background thread
- `FindByName<T>(parent, name)` (line 946) is the XAML element lookup helper

---

## Feature 1 — SC-GR-2: Text crossfade on track change

**What:** When the track changes (title text changes), fade the title and artist
`TextBlock`s from opacity 1 → 0, swap the text, then fade back 0 → 1.
Play/pause and other state-only updates must **not** trigger the fade.

**Where:** `ApplyStateToWidget()` starting at line 1098. The text-change branch
is already guarded by `if (titleTb.Text() != newTitle)` (line 1100).

**How:**
1. Add a `static Storyboard g_TextFadeStoryboard{ nullptr };` near `g_MarqueeStoryboard`
   (line 299 area).
2. Write a helper `FadeText(FrameworkElement el, double from, double to, double durationSec)`
   that creates a `DoubleAnimation` on `Opacity` and returns a `Storyboard`. Use
   `EasingDoubleKeyFrame` or simple `DoubleAnimation` — whichever is shorter. Duration:
   0.15 s out, 0.15 s in.
3. In the `if (titleTb.Text() != newTitle)` block (line 1100):
   - Stop any running `g_TextFadeStoryboard`
   - Animate title scroller (`kTitleScrollerName` StackPanel) and `artistTb` to opacity 0
     (0.15 s)
   - On the storyboard's `Completed` event: swap the text (`titleTb.Text(newTitle)`,
     `titleTb2.Text(newTitle)`, `artistTb.Text(artistDisplay)`), then start a second
     storyboard fading back to opacity 1 (0.15 s)
4. Remove the bare `titleTb.Text(newTitle)` / `titleTb2.Text()` calls at lines 1107–1108
   and the bare `artistTb.Text()` call at line 1113 **only when the text actually
   changes**. State-only updates (play/pause, color) must still write text directly
   without animation.
5. Guard the `artistTb.Text()` write at line 1113 so it only runs when the artist string
   has changed (same pattern as the title guard at line 1100), to avoid a spurious fade
   on play/pause.

**Constraints:**
- The fade storyboard's `Completed` lambda captures weak refs — `make_weak(titleTb)`,
  `make_weak(titleTb2)`, `make_weak(artistTb)` — and checks `.get()` before use.
- The marquee (`StartMarqueeIfNeeded`) is already called from `SizeChanged` after text
  is set; don't call it manually from the fade callback.
- `WH_CATCH` is not needed inside XAML event handlers — the caller (XAML) doesn't leak.

---

## Feature 2 — SC-SP-4: Widget fade in/out on show/hide

**What:** Instead of snapping `Visibility::Collapsed` / `Visibility::Visible`, fade
the widget's root `Opacity` smoothly. Duration: 0.2 s.

**Where:** Two show/hide sites:
1. `ApplyStateToWidget()` line 1298: `widget.Visibility(hasMedia ? Visible : Collapsed)`
2. Fullscreen poll dispatch at line 1831: `w.Visibility(Visibility::Collapsed)` and the
   `else` branch (line 1833) which calls `ApplyStateToWidget(w)` (which calls the above)

**How:**
1. Add a `static Storyboard g_WidgetFadeStoryboard{ nullptr };` near the other storyboard
   globals.
2. Write a helper:
   ```cpp
   static void SetWidgetVisible(UIElement el, bool visible) {
       if (g_WidgetFadeStoryboard) {
           try { g_WidgetFadeStoryboard.Stop(); } catch (...) {}
           g_WidgetFadeStoryboard = nullptr;
       }
       if (visible) {
           el.Visibility(Visibility::Visible);
           // animate opacity 0 → 1
       } else {
           // animate opacity 1 → 0; on Completed: set Visibility::Collapsed
       }
   }
   ```
3. Replace the two direct `Visibility` writes with `SetWidgetVisible(widget, hasMedia)`
   and `SetWidgetVisible(w, !hide)` respectively.
4. When fading in, ensure `el.Opacity(0.0)` is set **before** `Visibility::Visible` so
   there's no flash. When fading out, set `Visibility::Collapsed` only in the
   `Completed` callback so the widget isn't clipped mid-fade.
5. The `Completed` lambda for the fade-out captures a `weak_ref<UIElement>` and checks
   it before writing `Visibility`.

**Constraints:**
- `SetWidgetVisible` is called from the XAML dispatcher thread only (both call sites
  already dispatch via `RunAsync`). No thread safety needed inside the helper itself.
- Do not call `ApplyStateToWidget` from inside the fade `Completed` callback — the
  existing flow already calls it via `RefreshWidgetUI()`.

---

## Feature 3 — SC-HT-4: Smooth progress bar interpolation

**What:** The progress bar currently jumps when `ApplyStateToWidget` is called
(event-driven, irregular intervals). Add a `DispatcherTimer` that ticks every 500 ms
when the active session is playing, advancing the displayed position by elapsed wall
time so the fill moves smoothly between SMTC updates.

**Where:** New global timer; lightweight per-tick update touches only `kProgressFillName`
and `kTimestampName` — does **not** call `ApplyStateToWidget`.

**How:**
1. Add globals:
   ```cpp
   static DispatcherTimer g_ProgressTimer{ nullptr };
   static ULONGLONG       g_ProgressLastTickMs = 0;   // GetTickCount64() at last tick
   ```
2. Write `StartProgressTimer(Grid widget)`:
   - Creates a `DispatcherTimer` with `Interval` of 500 ms
   - `Tick` handler: reads `g_MediaStates[g_ActiveSessionIndex]` under `g_MediaMutex`
     to get `positionMs`, `durationMs`, `isPlaying`; if not playing, stop the timer
     and return. Otherwise compute `elapsed = GetTickCount64() - g_ProgressLastTickMs`,
     advance a local `displayPositionMs = positionMs + elapsed`, clamp to `durationMs`,
     update fill width and timestamp text, update `g_ProgressLastTickMs`.
   - The widget element is captured as `weak_ref<Grid>`.
3. Write `StopProgressTimer()`:
   ```cpp
   static void StopProgressTimer() {
       if (g_ProgressTimer) {
           try { g_ProgressTimer.Stop(); } catch (...) {}
           g_ProgressTimer = nullptr;
       }
   }
   ```
4. In `ApplyStateToWidget()`, after the progress bar width is written (line 1283):
   - If `isPlaying && durationMs > 0`: call `StopProgressTimer()`, set
     `g_ProgressLastTickMs = GetTickCount64()`, call `StartProgressTimer(widget)`.
   - If not playing or no duration: call `StopProgressTimer()`.
5. Call `StopProgressTimer()` from `Wh_ModUninit` alongside `StopMarquee()`.

**Constraints:**
- `g_MediaMutex` is the existing mutex protecting `g_MediaStates` — use it for the
  brief read inside the Tick handler.
- The `Tick` handler runs on the XAML dispatcher thread (DispatcherTimer guarantees
  this) — safe to touch XAML elements directly; no `RunAsync` needed.
- Do not advance `g_MediaStates[idx].positionMs` — that's SMTC ground truth. Only
  advance a local interpolated value for display.
- If `g_ActiveSessionIndex` changes between ticks (user cycles sessions), the timer
  will read the new session's data on the next tick. That's acceptable — the worst
  case is one tick of stale position data.

---

## Global State Summary (for quick reference)

```
g_MediaStates[MAX_SESSIONS]      — array of MediaState structs; index g_ActiveSessionIndex
g_MediaStateCount                — number of active sessions
g_ActiveSessionIndex             — which session is displayed
g_MediaMutex                     — mutex protecting g_MediaStates
g_WidgetRoot (weak_ref<Grid>)    — the injected widget root; acquire via g_WidgetMutex
g_WidgetMutex                    — mutex protecting g_WidgetRoot
g_MarqueeStoryboard (Storyboard) — existing pattern to follow for new storyboard globals
```

`MediaState` fields relevant here (line ~235):
```cpp
bool        isPlaying;
int64_t     positionMs;
int64_t     durationMs;
```

---

## Success Criteria

- **SC-GR-2:** Changing tracks produces a visible 0.15 s fade-out/in on title and artist.
  Pressing play/pause does NOT trigger a fade.
- **SC-SP-4:** Widget appears and disappears with a 0.2 s opacity ramp. No flash of
  invisible content; no layout shift during the fade.
- **SC-HT-4:** Progress fill and timestamp advance visibly every 500 ms while playing,
  without waiting for an SMTC event. Pausing stops the interpolation. The timer is
  cleaned up in uninit.

Compile target: MinGW (g++ with `-std=c++17`). All three features must compile without
warnings. Do not add or remove `#include` lines — all needed headers are present.
