# Implementation Prompt: Taskbar Media Widget — XAML Skeleton (Phase 1)

## Mission

Build the skeleton of a new Windhawk mod (`taskbar-media-widget.wh.cpp`) that injects a
media widget natively into the Windows 11 taskbar XAML tree. Phase 1 deliverable is a
**working skeleton only** — injection proven, GSMTC connected, real data visible, multi-session
architecture in place. No polish, no flyout, no seek bar, no album art yet.

Output file: `/Users/dante/Citadel/native-taskbar-media-controller/taskbar-media-widget.wh.cpp`

Do not modify any file under `/forks/`. Do not modify the existing
`taskbar-music-lounge-ae.wh.cpp`. The new file is a clean parallel build.

---

## Architecture Decision (locked)

**Architecture: Native XAML injection into `Grid#RootGrid` under `Taskbar.TaskbarFrame`.**

This is not an overlay window. There is no `CreateWindow`, no `WS_EX_LAYERED`, no
`SetLayeredWindowAttributes`, no `UpdateLayeredWindow`, no `RegisterTaskbarHook`. The widget
lives inside Explorer's own XAML tree as a direct child of `Grid#RootGrid`.

This is the architecture the Windhawk maintainer (m417z) explicitly prefers. It gives
auto-hide support, correct z-ordering, and taskbar anchoring for free.

### Proven reference implementation

bbmaster123's `tb-video-injector.cpp` demonstrates frame-level injection working in
production. Read it before writing any code:

```
https://raw.githubusercontent.com/bbmaster123/FWFU/main/Personal%20Windhawk%20Mods/tb-video-injector.cpp
```

Key patterns to adopt directly from that file:
- `LoadLibraryExW` hook for deferred `Taskbar.View.dll` loading
- `TaskListButton::UpdateVisualStates` as entry hook
- `GetFrameworkElementFromNative`: `(void**)pThis + 3` ABI offset + `winrt::copy_from_abi`
- `ScheduleScanAsync`: walk up to `Taskbar.TaskbarFrame`, walk down to `RootGrid`,
  dispatch injection on UI thread via `Dispatcher().RunAsync()`
- `winrt::weak_ref<>` for all cached element references
- Backward iteration (`i--`) when removing from `Children` collections
- `SizeChanged` deferred injection when `ActualWidth == 0` on first call

Do not copy the video/media player code. Only adopt the injection scaffold.

---

## Taskbar XAML Tree (confirmed structure)

```
Taskbar.TaskbarFrame
└── Grid#RootGrid
    ├── Taskbar.TaskbarBackground        (acrylic background, z=0)
    ├── ItemsRepeater#TaskbarFrameRepeater  (app buttons, z=1)
    └── Grid#SystemTrayFrameGrid         (clock + tray icons, rightmost)
```

The widget injects as a new child of `Grid#RootGrid`. Position it using:
- `HorizontalAlignment::Right`
- `VerticalAlignment::Stretch`
- Right margin wide enough to clear `Grid#SystemTrayFrameGrid` (~200px initial estimate;
  make this a setting `OffsetX` defaulting to 200)
- Fixed width from settings (`PanelWidth`, default 300)
- `Canvas::SetZIndex` above background (z=2) but the interaction layer is standard hit-test

---

## Phase 1 Deliverable — Skeleton

The mod must do exactly these things and nothing more:

### 1. Inject a placeholder panel into RootGrid

A `Grid` named `"TaskbarMediaWidgetRoot"` injected as a child of `Grid#RootGrid`.
Initial appearance:
- Background: semi-transparent dark brush (`#CC1A1A1A`)
- CornerRadius: 8
- Fixed width from `PanelWidth` setting (default 300)
- Right-aligned with `OffsetX` right margin (default 200)
- Vertically centered (`VerticalAlignment::Center`), height from `PanelHeight` setting
  (default 40, slightly less than taskbar)

If the panel is visible and positioned correctly after mod load: injection is working.

### 2. Connect GSMTC with multi-session array

Implement the multi-session architecture from day one. Use a fixed-size array:

```cpp
static constexpr int MAX_SESSIONS = 10;
struct MediaState {
    std::wstring title;
    std::wstring artist;
    std::wstring sessionId;   // AUMID
    bool isPlaying = false;
    int64_t positionMs = 0;
    int64_t durationMs = 0;
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession session{ nullptr };
};
MediaState g_MediaStates[MAX_SESSIONS];
int g_MediaStateCount = 0;
int g_ActiveSessionIndex = 0;
std::mutex g_MediaMutex;
```

Session enumeration:
- Use `GlobalSystemMediaTransportControlsSessionManager::RequestAsync()` on a background
  thread
- Iterate `GetSessions()`, populate `g_MediaStates[]`
- Set `g_ActiveSessionIndex` to the session with `PlaybackStatus == Playing`; if multiple
  playing, prefer the first; if none playing, prefer index 0
- Subscribe to `SessionsChanged` on the manager and `MediaPropertiesChanged` +
  `PlaybackInfoChanged` on each session
- All XAML updates must be marshalled back to the UI thread via `PostMessage` to a
  message-only HWND or via the panel's `Dispatcher().RunAsync()`

### 3. Display title and artist as TextBlocks

Inside `TaskbarMediaWidgetRoot`, a horizontal `StackPanel` containing:
- `TextBlock#NowPlayingTitle` — track title, white, font size from `FontSize` setting
  (default 11), trimmed with ellipsis, max width ~180px
- `TextBlock#NowPlayingArtist` — artist name, 70% opacity white, same font size, trimmed

If no session is active or no media is playing: collapse the panel
(`Visibility::Collapsed`). Do not show "No media playing" text in Phase 1.

### 4. Play/Pause and Skip buttons

Two `Button` controls:
- Play/Pause: toggles `▶` / `⏸` label; calls `TryTogglePlayPauseAsync()` on the active
  session
- Next track: `⏭` label; calls `TrySkipNextAsync()` on the active session

No previous-track button in Phase 1. Keep it minimal.

Use `AutomationProperties.Name` on each button for accessibility.

### 5. Multi-session switcher (minimal)

If `g_MediaStateCount > 1`, show a small `TextBlock` displaying `"2"` / `"3"` etc
(session count) to the left of the title. Clicking it cycles `g_ActiveSessionIndex` and
refreshes the display. No dropdown, no session list UI in Phase 1 — just the cycle-on-click
pattern.

---

## Compiler Options

```cpp
// @compilerOptions -lole32 -loleaut32 -lruntimeobject -lwindowsapp -lversion -DWINVER=0x0A00
// @include         explorer.exe
// @architecture    x86-64
```

---

## Symbols to Hook (Taskbar.View.dll)

All via `WindhawkUtils::SYMBOL_HOOK` fuzzy matching:

```
private: void __cdecl winrt::Taskbar::implementation::TaskListButton::UpdateVisualStates(void)
```
Entry point. Fires on hover/state change. Use to walk up to TaskbarFrame and inject.

```
public: virtual int __cdecl winrt::impl::produce<struct winrt::Taskbar::implementation::TaskbarFrame,struct winrt::Windows::UI::Xaml::IFrameworkElementOverrides>::MeasureOverride(struct winrt::Windows::Foundation::Size,struct winrt::Windows::Foundation::Size *)
```
Secondary trigger. Fires on layout recalculation (resolution change, DPI change). Use to
re-validate widget position.

Both hooks call the same `ScheduleScanAsync` path.

---

## Settings Schema (Phase 1 only)

```
- PanelWidth: 300
  $name: Widget width (px)
- PanelHeight: 40
  $name: Widget height (px)
- FontSize: 11
  $name: Font size
- OffsetX: 200
  $name: Right margin from edge (px) — adjust to clear system tray
- HideFullscreen: true
  $name: Hide when fullscreen
```

---

## Fullscreen Hiding

If `HideFullscreen` is true, poll `SHQueryUserNotificationState()` on the existing
`IDT_POLL_MEDIA` timer (1000ms). Set `Visibility::Collapsed` on
`TaskbarMediaWidgetRoot` when `QUNS_RUNNING_D3D_FULL_SCREEN` is returned.

---

## What Is Explicitly Out of Scope for Phase 1

Do not implement any of the following. Leave no stubs or TODOs for them either —
clean absence is better than dead code:

- Album art rendering
- Seek bar / progress bar
- Flyout popup panel
- Blurred background
- Adaptive text color
- FFT audio visualizer (permanently out of scope for this mod version)
- Rainbow border
- Caps lock overlay
- LRC lyrics
- Volume control
- Per-app volume
- `BringSourceAppToFront` on click
- Persistent widget position
- `AutoScrollTitle`
- Two-line text layout
- Widget fade animation
- Text crossfade

---

## Success Criteria for Phase 1

The skeleton is complete when ALL of the following are true:

1. Mod loads in Windhawk targeting `explorer.exe` without crashing Explorer
2. A dark rounded panel is visible in the taskbar, right-aligned, left of the clock area
3. The panel shows real track title and artist from whatever is currently playing on the
   system (Spotify, YouTube Music, Windows Media Player — anything GSMTC-exposed)
4. Play/pause button toggles playback on the active GSMTC session
5. Next button skips the track
6. Panel collapses when no media is playing
7. Panel hides when a fullscreen app is running (if HideFullscreen=true)
8. If two media apps are playing simultaneously, the session counter appears and cycling
   works
9. Mod unloads cleanly — panel is removed from the XAML tree, no zombie references

---

## Reference Files (read-only, do not modify)

- Baseline mod: `/Users/dante/Citadel/native-taskbar-media-controller/forks/og_Hashah2311_taskbar-music-lounge.wh/og_Hashah2311_taskbar-music-lounge.wh.cpp`
  — reference for GSMTC consumption pattern and settings schema conventions
- Messij's multi-session fork: `/Users/dante/Citadel/native-taskbar-media-controller/forks/messij/taskbar-music-lounge-multiple/mod.wh.cpp`
  — reference for `g_MediaStates[]` array pattern and session enumeration
- Synthesis report: `/Users/dante/Citadel/native-taskbar-media-controller/fork-reports/synthesis-2026-05-19.md`
  — full candidate list and conflict notes; consult if unsure about a design choice
- bbmaster123 scaffold: fetch from URL above before starting; do not commit it to the repo

---

## When Phase 1 Is Done

Report back with:
1. The file path of the written mod
2. Which success criteria passed and which (if any) did not
3. Any symbols that failed to resolve and what fallback was used
4. The actual right-margin value needed to clear the system tray (the OffsetX default
   may need calibrating)
5. Any GSMTC session enumeration behavior that differed from expectation

Do not proceed to Phase 2 features without operator sign-off.
