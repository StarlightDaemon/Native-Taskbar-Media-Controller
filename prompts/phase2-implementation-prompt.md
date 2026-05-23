# Phase 2 Implementation Prompt

**Task:** Implement Phase 2 features in `native-taskbar-media-controller.wh.cpp`. Four candidates. Do not restructure or refactor anything outside the scope of each candidate.

---

## Context

This is a Windhawk mod that injects a native XAML media widget into the Windows 11 taskbar. **It is not a GDI+/layered-window mod.** All UI uses WinRT XAML types (`Grid`, `TextBlock`, `Button`, `Image`, `ProgressBar`, etc.) running on the UI dispatcher thread. Off-thread state changes call `RefreshWidgetUI()` which dispatches via `widget.Dispatcher().RunAsync(...)`. Never access XAML elements off the UI thread.

All async tasks must:
- Increment `g_AsyncTasks++` on entry, decrement via RAII guard on exit
- Check `g_Unloading.load()` after every `co_await`
- Wrap WinRT calls in `WH_CATCH` or try/catch

**Already implemented — do not re-implement:**
- `g_MediaStates[10]` multi-session array; `sessionId` (AUMID) per slot — line 174+
- `IsForegroundWindowFullscreen` with monitor-coverage — line 1083
- `WH_CATCH` / `WH_TRY_OR` macros — line 111+
- `positionMs` and `durationMs` fields already exist in `MediaState` (lines 184–185) but are not yet populated from GSMTC

---

## Candidate 1 — SC-CH-1: Auto-hide taskbar detection (Hashah2311 & Chaython)

**Add this function:**
```cpp
// SC-CH-1: IsTaskbarEffectivelyVisible — Hashah2311 & Chaython
// Returns false when the taskbar has auto-hidden to ≤30px visible strip.
static bool IsTaskbarEffectivelyVisible(HWND hTaskbar) {
    if (!hTaskbar || !IsWindowVisible(hTaskbar)) return false;
    RECT rc; GetWindowRect(hTaskbar, &rc);
    HMONITOR hMon = MonitorFromWindow(hTaskbar, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (GetMonitorInfo(hMon, &mi)) {
        RECT intersect;
        if (IntersectRect(&intersect, &rc, &mi.rcMonitor)) {
            int visW = intersect.right  - intersect.left;
            int visH = intersect.bottom - intersect.top;
            if (visW <= 30 || visH <= 30) return false;
        } else { return false; }
    }
    return true;
}
```

**Wire it into `FullscreenPollThread`**: add `!IsTaskbarEffectivelyVisible(hTaskbar)` to the `hide` condition so the widget hides when the taskbar slides off-screen. `hTaskbar` is already available in that scope (`g_hTaskbarWnd.load()`).

---

## Candidate 2 — SC-UI-2: Adaptive text color from album art luminance (Uiisland)

**Add to includes** (not currently present):
```cpp
#include <winrt/Windows.Graphics.Imaging.h>
```
Add `using namespace Windows::Graphics::Imaging;` alongside the existing using declarations.

**Add `isDarkCover` to `MediaState`:**
```cpp
bool isDarkCover = false;
```

**In the `fire_and_forget` art-loading lambda inside `ApplyStateToWidget`**, after the `BitmapImage` is decoded and set:
- Open a second stream from `ref` (calling `OpenReadAsync()` again is safe — `IRandomAccessStreamReference` creates a fresh stream each call)
- `co_await BitmapDecoder::CreateAsync(stream2)` → `decoder`
- `co_await decoder.GetSoftwareBitmapAsync(BitmapPixelFormat::Bgra8, BitmapAlphaMode::Premultiplied)` → `sbmp`
- Sample the center pixel for B/G/R: use `sbmp.LockBuffer(BitmapBufferAccessMode::Read)` + `IMemoryBufferReference`
- Compute ITU-R BT.601 luma: `luma = 0.299f*R + 0.587f*G + 0.114f*B`
- Determine `dark = (luma < 135.0f)`
- Under `g_MediaMutex`, write `g_MediaStates[activeIdx].isDarkCover = dark` (capture `activeIdx` in the lambda)
- Call `RefreshWidgetUI()`

**In `ApplyStateToWidget`**, read `isDarkCover` alongside the other fields under the mutex. Set foreground brushes:
- Dark cover (`isDarkCover == true`): title white `0xFF,0xFF,0xFF,0xFF`, artist `0xB3,0xFF,0xFF,0xFF` (existing defaults — no change)
- Light cover (`isDarkCover == false`): title `0xFF,0x1A,0x1A,0x1A`, artist `0xB3,0x1A,0x1A,0x1A`

**Gate with a new setting** `AdaptiveTextColor` (bool, default true). Only run the extraction and change foreground if the setting is enabled.

---

## Candidate 3 — SC-M-2: BringSourceAppToFront (Messij)

**Add to `@compilerOptions`:** `-lpropsys`

**Add includes** if not already present:
```cpp
#include <propsys.h>
#include <propkey.h>
```

**Port these four functions** from the Messij fork — adapt from GDI+ WndProc style to free functions:

`ExtractExeHint(const std::wstring& aumid) → std::wstring`
Parses AUMID (e.g. `L"Spotify.exe!App"`) to extract a short exe-name hint (`L"Spotify"`). Strip everything after `!`, strip `.exe` suffix, lowercase.

`FindWindowByAppIdProc(HWND hwnd, LPARAM lParam) → BOOL CALLBACK`
`EnumWindows` callback. Uses `SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&store))` → `store->GetValue(PKEY_AppUserModel_ID, &pv)` to compare AUMID. Falls back to exe-name hint match via `GetWindowThreadProcessId` + `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION)` + `GetModuleFileNameExW`. Scores and stores best candidate HWND in `LPARAM`-passed struct.

`FindBestWindowProc(HWND hwnd, LPARAM lParam) → BOOL CALLBACK`
Secondary scoring pass if primary yields no result — picks the largest visible window belonging to the process.

`BringSourceAppToFront(const std::wstring& aumid)`
Calls `EnumWindows(FindWindowByAppIdProc, ...)`. If a window found: `SetForegroundWindow` + `ShowWindow(SW_RESTORE)` if minimized.

**Add `DoubleTapped` handler on `root` in `BuildWidget()`:**
```cpp
root.DoubleTapped(DoubleTappedEventHandler(
    [](IInspectable const&, DoubleTappedRoutedEventArgs const& e) {
        std::wstring aumid;
        {
            std::lock_guard<std::mutex> g(g_MediaMutex);
            if (g_ActiveSessionIndex >= 0 && g_ActiveSessionIndex < g_MediaStateCount)
                aumid = g_MediaStates[g_ActiveSessionIndex].sessionId;
        }
        if (!aumid.empty()) BringSourceAppToFront(aumid);
        e.Handled(true);
    }));
```

`EnumWindows` is synchronous and fast — calling from the UI thread dispatcher is fine for this purpose.

---

## Candidate 4 — SC-KV-4: Display-only track progress bar (kevinoe)

**Populate `positionMs` and `durationMs` in `UpdateOneSessionAsync`**, after the `GetPlaybackInfo()` block:
```cpp
try {
    auto tl = session.GetTimelineProperties();
    if (tl) {
        auto pos   = tl.Position();
        auto end   = tl.EndTime();
        auto start = tl.StartTime();
        int64_t dur = (end - start).count() / 10'000;  // 100ns → ms
        int64_t pms = pos.count() / 10'000;
        // Only flag changed if delta > 500ms to avoid RefreshWidgetUI on
        // every 1s poll tick during normal playback.
        bool posChanged = std::abs(m.positionMs - pms) > 500 || m.durationMs != dur;
        m.positionMs = pms;
        m.durationMs = dur;
        // posChanged intentionally not used to trigger an extra RefreshWidgetUI —
        // position is read fresh in ApplyStateToWidget on each existing refresh.
    }
} WH_CATCH(L"UpdateOneSessionAsync/Timeline")
```

**Add `kProgressBarName` constexpr** and a `ProgressBar` to `BuildWidget()`:
- Place it directly in the `root` Grid (not in the `StackPanel`), `VerticalAlignment::Bottom`, `Height(3)`, full width
- `Minimum(0)`, `Maximum(100)`, `Value(0)`, `IsIndeterminate(false)`
- `Background(MakeBrush(0x00, 0, 0, 0))` (transparent track), `Foreground(MakeBrush(0x99, 0xFF, 0xFF, 0xFF))` (semi-white fill)
- `Visibility(Visibility::Collapsed)` initially

**In `ApplyStateToWidget`**, read `positionMs` and `durationMs` under the mutex alongside other fields. Find the `ProgressBar` by name:
```cpp
if (auto pb = FindByName<ProgressBar>(widget, kProgressBarName)) {
    if (durationMs > 0) {
        pb.Value(positionMs * 100.0 / durationMs);
        pb.Visibility(Visibility::Visible);
    } else {
        pb.Visibility(Visibility::Collapsed);
    }
}
```

**Gate with a new setting** `ShowProgress` (bool, default true).

---

## Settings block additions

Add to `// ==WindhawkModSettings==`:
```yaml
- ShowProgress: true
  $name: Show track progress bar
- AdaptiveTextColor: true
  $name: Adaptive text color (matches album art brightness)
```

Add to `ModSettings` struct and `LoadSettings()`. In `Wh_ModSettingsChanged`, re-apply both settings (call `RefreshWidgetUI()` after loading if either changes).

---

## Version bump

`@version 0.1.0-beta.2.8` → `@version 0.2.0-beta.1`

Update the Readme features list to mention: auto-hide support, adaptive text color, double-click to focus source app, track progress bar.

---

## What NOT to do

- Do not port `CS_DBLCLKS` (GDI+ concept, N/A to XAML)
- Do not change `FullscreenPollThread` interval or drain timeouts
- Do not alter `Wh_ModInit` cold-boot threading logic
- Do not add comments explaining what code does — only add comments where the WHY is non-obvious (e.g. the `>500ms` position threshold)
- Do not create new files
- Do not remove `BootLog` calls — retained through v1.0.0 by policy
