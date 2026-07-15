# Implementation Handoff: Marquee Scroll for Title TextBlock
**Project:** Native Taskbar Media Controller (Windhawk mod)  
**Date:** 2026-05-24  
**Agent role:** Implement the marquee scroll feature. Edit `native-taskbar-media-controller.wh.cpp` only. Do not touch any other file.  
**Primary source file:** `native-taskbar-media-controller.wh.cpp` (root of this repo)

---

## Context

This is a Windhawk mod injected into `explorer.exe`. The widget is a native XAML subtree
inserted as a real child of `Grid#RootGrid` under `Taskbar.TaskbarFrame`. No separate HWND.
Compiled with MinGW C++/WinRT. All UI mutations must happen on the XAML UI dispatcher thread
via `Dispatcher().RunAsync(...)`. Global widget reference is `g_WidgetRoot` (a `weak_ref<Grid>`).

The Animation namespace is already imported (line 142–157):
```cpp
#include <winrt/Windows.UI.Xaml.Media.Animation.h>
using namespace Windows::UI::Xaml::Media::Animation;
```

---

## What exists today (baseline)

### XAML structure (built in `InjectWidgetInto()`, line 1160)

```
Grid root                           ← widget root; HorizontalAlignment::Right
  └─ Grid layout                    ← 6-column grid; col 2 is GridUnitType::Star
       ├─ col 0: Image albumArt     ← Auto width
       ├─ col 1: TextBlock sessionCount  ← Auto width
       ├─ col 2: StackPanel textCol ← Star (fills remaining space)
       │    ├─ TextBlock title      ← TextTrimming::CharacterEllipsis, NoWrap, MaxLines(1)  [line 654–662]
       │    ├─ TextBlock artist     ← TextTrimming::CharacterEllipsis, NoWrap, MaxLines(1)
       │    └─ TextBlock timestamp  ← Visibility::Collapsed when no timeline
       ├─ col 3: Button skipBack    ← Auto
       ├─ col 4: Button playPause   ← Auto
       └─ col 5: Button skipFwd     ← Auto
```

### Title TextBlock construction (lines 654–662):
```cpp
TextBlock title;
title.Name(kTitleName);
title.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
title.FontSize((double)g_Settings.fontSize);
title.TextTrimming(TextTrimming::CharacterEllipsis);
title.TextWrapping(TextWrapping::NoWrap);
title.MaxLines(1);

textCol.Children().Append(title);
```

### Title text is set in `ApplyStateToWidget()` (line 895, relevant section ~line 950):
```cpp
auto titleTb = FindByName<TextBlock>(widget, kTitleName);
// ...
if (titleTb)
    titleTb.Text(hasMedia ? title : L"");
```

### Globals (lines 260–284):
```cpp
static std::atomic<int> g_AsyncTasks{ 0 };
static weak_ref<Grid>             g_WidgetRoot{ nullptr };
static weak_ref<Grid>             g_RootGrid{ nullptr };
static weak_ref<FrameworkElement> g_SystemTray{ nullptr };
```

### Settings struct (line 188–197):
```cpp
struct ModSettings {
    int panelWidth = 300;
    int panelHeight = 40;
    int fontSize = 11;
    int offsetX = 8;
    bool hideFullscreen = true;
    bool showProgress = true;
    bool adaptiveTextColor = true;
    int backgroundStyle = 1;
} g_Settings;
```

### Unload function (`Wh_ModUninit`, line 1804):
```cpp
void Wh_ModUninit() {
    g_Unloading = true;
    // ... signals threads, waits ...
    RemoveWidget();
    // ... drains g_HookCallCounter and g_AsyncTasks ...
}
```

---

## Confirmed constraints (from research audit)

1. **No `ClipToBounds` in WinRT XAML.** Use `UIElement.Clip` with a `RectangleGeometry` instead. A `Canvas` wrapper is the correct container because it does not constrain its children's layout width — the `TextBlock` inside can expand to its natural unconstrained width.

2. **`TextTrimming::CharacterEllipsis` must be removed from the title TextBlock.** With it active, `ActualWidth` reports the clipped column width, not the text's natural width. Overflow is undetectable.

3. **`TranslateTransform` must be set at construction.** `Storyboard` targeting `TranslateTransform.X` fails silently if `RenderTransform` is null at animation start.

4. **MinGW cannot link `TranslateTransform::XProperty()` static accessor.** Use string-based property path: `"(UIElement.RenderTransform).(TranslateTransform.X)"`.

5. **Do not use `LayoutUpdated`.** It fires globally for every XAML layout pass in the island (clock ticks, tray updates) and creates feedback loops.

6. **Use `SizeChanged` on the title TextBlock** as the layout-complete signal to measure overflow and (re)start animation.

7. **Cannot stop + restart a storyboard in the same `RunAsync` callback that sets new text.** Layout hasn't run yet; `ActualWidth` still reflects old text. The `SizeChanged` event fires after layout and is the correct restart point.

8. **Storyboard holds a strong ref to its target.** Must call `Stop()` explicitly before removing the widget (in `RemoveWidget()` and on mod unload).

---

## Required changes — step by step

### Step 1 — Add a `marqueeScroll` bool to `ModSettings` and `LoadSettings()`

Add field to `ModSettings` struct (line ~196):
```cpp
bool marqueeScroll = true;
```

Add load in `LoadSettings()` (after the existing `adaptiveTextColor` load, line ~206):
```cpp
g_Settings.marqueeScroll = Wh_GetIntSetting(L"MarqueeScroll") != 0;
```

Add the setting declaration in the mod metadata block at the top of the file (in the `@setting` JSDoc block, alongside the other settings). Pattern: look for `// @setting` lines near the top of the file.

### Step 2 — Add a name constant for the Canvas wrapper

Near the other `constexpr` name constants (lines 267–276):
```cpp
constexpr std::wstring_view kTitleCanvasName = L"NowPlayingTitleCanvas";
```

### Step 3 — Add marquee globals

Near the other static globals (after line 284):
```cpp
static Storyboard           g_MarqueeStoryboard{ nullptr };
static winrt::event_token   g_TitleSizeChangedToken{};
```

### Step 4 — Add a `StopMarquee()` helper function

Add before `ApplyStateToWidget()` (before line 895). This function must be callable from
both `RemoveWidget()` and the SizeChanged restart path:

```cpp
static void StopMarquee() {
    if (g_MarqueeStoryboard) {
        try { g_MarqueeStoryboard.Stop(); } catch (...) {}
        g_MarqueeStoryboard = nullptr;
    }
}
```

### Step 5 — Add a `StartMarqueeIfNeeded()` helper function

Add after `StopMarquee()`. This is called from the `SizeChanged` handler:

```cpp
static void StartMarqueeIfNeeded(Canvas titleCanvas, TextBlock titleTb) {
    StopMarquee();

    double canvasW = titleCanvas.ActualWidth();
    double textW   = titleTb.ActualWidth();
    if (canvasW <= 0 || textW <= canvasW) {
        // No overflow — reset position and leave static
        if (auto tt = titleTb.RenderTransform().try_as<TranslateTransform>())
            tt.X(0.0);
        return;
    }

    double overhang = textW - canvasW;
    double scrollDuration = overhang / 40.0;  // 40 px/s

    TranslateTransform tt = titleTb.RenderTransform().as<TranslateTransform>();
    tt.X(0.0);

    DoubleAnimation anim;
    anim.From(0.0);
    anim.To(-overhang);
    anim.Duration(DurationHelper::FromTimeSpan(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::duration<double>(scrollDuration)).count() * 100LL
        // WinRT TimeSpan is in 100-nanosecond intervals
    ));
    anim.BeginTime(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::seconds(2)).count() * 100LL);  // 2s pause before scroll
    anim.AutoReverse(false);
    anim.RepeatBehavior(RepeatBehaviorHelper::Forever());

    Storyboard sb;
    Storyboard::SetTarget(anim, titleTb);
    Storyboard::SetTargetProperty(anim,
        PropertyPath(L"(UIElement.RenderTransform).(TranslateTransform.X)"));
    sb.Children().Append(anim);

    g_MarqueeStoryboard = sb;
    sb.Begin();
}
```

**Note on TimeSpan construction:** WinRT `Windows::Foundation::TimeSpan` is a `std::chrono::duration<int64_t, std::ratio<1, 10'000'000>>`. Use:
```cpp
TimeSpan MakeTimeSpan(double seconds) {
    return TimeSpan{ static_cast<int64_t>(seconds * 10'000'000) };
}
```
Add this as a small helper alongside `StopMarquee`.

Revise `StartMarqueeIfNeeded` to use `MakeTimeSpan`:
```cpp
anim.Duration(DurationHelper::FromTimeSpan(MakeTimeSpan(scrollDuration)));
anim.BeginTime(MakeTimeSpan(2.0));  // 2s start pause
```

### Step 6 — Modify `InjectWidgetInto()` — wrap title in Canvas

Replace the title TextBlock construction and append (lines 654–662) with:

```cpp
// Title clip canvas
Canvas titleCanvas;
titleCanvas.Name(kTitleCanvasName);
titleCanvas.HorizontalAlignment(HorizontalAlignment::Stretch);
titleCanvas.VerticalAlignment(VerticalAlignment::Center);
titleCanvas.Height((double)g_Settings.fontSize * 1.6);  // enough for one line

// Clip geometry — width updated dynamically via SizeChanged
RectangleGeometry clipRect;
clipRect.Rect(RectHelper::FromCoordinatesAndDimensions(0, 0, 0, (float)(g_Settings.fontSize * 1.6)));
titleCanvas.Clip(clipRect);

TextBlock title;
title.Name(kTitleName);
title.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
title.FontSize((double)g_Settings.fontSize);
title.TextTrimming(TextTrimming::None);         // ← CHANGED: was CharacterEllipsis
title.TextWrapping(TextWrapping::NoWrap);
title.MaxLines(1);
title.VerticalAlignment(VerticalAlignment::Center);

// TranslateTransform required before Storyboard can target it
TranslateTransform titleTranslate;
title.RenderTransform(titleTranslate);

titleCanvas.Children().Append(title);

// SizeChanged on the Canvas keeps the clip rect width current;
// SizeChanged on the TextBlock drives overflow detection and marquee restart.
titleCanvas.SizeChanged(SizeChangedEventHandler(
    [](IInspectable const& sender, SizeChangedEventArgs const& e) {
        auto canvas = sender.as<Canvas>();
        if (auto clip = canvas.Clip().try_as<RectangleGeometry>()) {
            auto r = clip.Rect();
            clip.Rect(RectHelper::FromCoordinatesAndDimensions(
                0, 0, (float)e.NewSize().Width, r.Height));
        }
    }));

auto weakCanvas = make_weak(titleCanvas);
auto weakTitle  = make_weak(title);
g_TitleSizeChangedToken = title.SizeChanged(SizeChangedEventHandler(
    [weakCanvas, weakTitle](IInspectable const&, SizeChangedEventArgs const&) {
        auto cv = weakCanvas.get();
        auto tb = weakTitle.get();
        if (!cv || !tb) return;
        if (g_Settings.marqueeScroll)
            StartMarqueeIfNeeded(cv, tb);
    }));

textCol.Children().Append(titleCanvas);
```

**Important:** The `g_TitleSizeChangedToken` assignment means the token must be revoked before the widget is removed (see Step 7).

### Step 7 — Revoke token and stop storyboard on widget removal

In `RemoveWidget()` (line 1255), before removing the widget from the tree, add:

```cpp
// Stop marquee and revoke SizeChanged token
StopMarquee();
if (g_TitleSizeChangedToken.value) {
    // Find title TextBlock and revoke token
    if (auto widget = g_WidgetRoot.get()) {
        if (auto tb = FindByName<TextBlock>(widget, kTitleName))
            tb.SizeChanged(g_TitleSizeChangedToken);
    }
    g_TitleSizeChangedToken = {};
}
```

### Step 8 — Stop storyboard on title text update

In `ApplyStateToWidget()` (line 895), when setting new title text (~line 950–951):

```cpp
if (titleTb) {
    StopMarquee();   // ← ADD: stop current animation before text changes
    if (auto tt = titleTb.RenderTransform().try_as<TranslateTransform>())
        tt.X(0.0);   // ← ADD: reset scroll position
    titleTb.Text(hasMedia ? title : L"");
    // SizeChanged will fire after layout and call StartMarqueeIfNeeded
}
```

### Step 9 — Add `@setting` metadata for MarqueeScroll

In the mod metadata JSDoc block at the top of the file, add alongside the other settings:
```
// @setting {"id": "MarqueeScroll", "type": "boolean", "defaultValue": true, "title": "Scroll long titles", "description": "Marquee-scroll title text that is wider than the available space"}
```

---

## What NOT to do

- Do not use `LayoutUpdated` anywhere.
- Do not call `Measure()` manually on the title TextBlock.
- Do not use `DispatcherTimer`.
- Do not use `TranslateTransform::XProperty()` as a static accessor — use the string path.
- Do not add marquee to the artist TextBlock in this pass — title only.
- Do not set `TextTrimming::CharacterEllipsis` back on the title TextBlock.
- Do not change any other files.

---

## Verification checklist

After implementing, check:
- [ ] Title text that fits within the column is static (no animation, no jitter)
- [ ] Title text that overflows scrolls after a 2s pause at 40 px/s
- [ ] Changing tracks (new title) stops the running animation and resets position
- [ ] MarqueeScroll = false leaves the title at `TextTrimming::None` but never starts animation
- [ ] Artist and timestamp TextBlocks are unchanged
- [ ] `Wh_ModUninit` completes cleanly without crashing (`RemoveWidget` runs, storyboard stopped)
- [ ] No compilation errors under the MinGW toolchain

---

## File to edit

`E:\Citadel/native-taskbar-media-controller/native-taskbar-media-controller.wh.cpp`
