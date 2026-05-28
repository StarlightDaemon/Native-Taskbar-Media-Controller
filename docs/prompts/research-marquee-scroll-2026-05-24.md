# Research Prompt: Marquee Scroll for Title TextBlock
**Project:** Native Taskbar Media Controller (Windhawk mod)  
**Date:** 2026-05-24  
**Agent role:** Research and analysis only — no code changes. Return a structured findings report.  
**Primary source file:** `native-taskbar-media-controller.wh.cpp` (root of this repo)

---

## Context

This is a Windhawk mod that injects a native XAML widget as a real child element of the
Windows 11 taskbar XAML tree (`Grid#RootGrid` under `Taskbar.TaskbarFrame`). It runs
inside `explorer.exe`, compiled with the Windhawk MinGW toolchain targeting WinRT
C++/WinRT XAML. There is no separate HWND, no WPF, no UWP app model. The XAML tree is
borrowed from the live taskbar process.

---

## The Problem

Title text that is longer than the available column space should scroll horizontally
(marquee-style). This feature has been attempted multiple times and has not landed in
the shipped code. The current state is:

- The title `TextBlock` has `TextTrimming::CharacterEllipsis` and `TextWrapping::NoWrap`
  (lines ~658–660) — text is statically clipped with an ellipsis. No animation exists.
- The `Windows::UI::Xaml::Media::Animation` namespace is imported (line 142, line 157)
  but is not used anywhere in the file.
- Two approaches were described in planning documents as "attempted" but neither is
  present in the compiled code:
  1. **DispatcherTimer at 16 ms** driving a `TranslateTransform` on the title
     `TextBlock` inside a `Border(ClipToBounds)`, with a 2s start-pause → 40px/s
     scroll → 1s end-pause → instant reset cycle.
  2. **`LayoutUpdated` event** triggering a `Storyboard` / `DoubleAnimation` on the
     title `TextBlock`'s `RenderTransform`, firing only when text overflows the clip
     container.

The goal of this research prompt is to understand *why* these approaches fail in this
specific environment, and what a correct approach would require conceptually.

---

## Current XAML Structure (relevant section)

Read the source file to verify, but the structure as built in `InjectWidgetInto()`
(around line 585) is:

```
Grid root                          ← widget root; HorizontalAlignment::Right
  └─ Grid layout                   ← 6-column grid; col 2 is GridUnitType::Star
       ├─ col 0: Image albumArt    ← Auto width
       ├─ col 1: TextBlock sessionCount  ← Auto width
       ├─ col 2: StackPanel textCol     ← Star (fills remaining space)
       │    ├─ TextBlock title     ← TextTrimming::CharacterEllipsis, NoWrap, MaxLines(1)
       │    ├─ TextBlock artist    ← TextTrimming::CharacterEllipsis, NoWrap, MaxLines(1)
       │    └─ TextBlock timestamp ← collapsed when no timeline
       ├─ col 3: Button skipBack   ← Auto
       ├─ col 4: Button playPause  ← Auto
       └─ col 5: Button skipFwd    ← Auto
```

The `textCol` StackPanel is in the star column (col 2). Its `ActualWidth` at runtime
equals whatever space is left after the Auto columns consume their natural widths.

---

## Research Questions

Work through each section below. Read the actual source file where directed. Return a
structured report — no code changes, no edits to the repo.

---

### Section 1 — Why overflow detection is the hard part

The core challenge is detecting whether the title text is longer than the available
column width. Answer these questions based on WinRT XAML behavior:

1. With `TextTrimming::CharacterEllipsis` active, the `TextBlock` clips its own content
   to fit within its measured width. Does this mean the `TextBlock.ActualWidth` reflects
   the *constrained* width (the column's available width) rather than the *natural* width
   of the text? If so, what property or method would give you the text's natural
   (unconstrained) width?

2. `FrameworkElement::Measure(Size)` called with an unconstrained width
   (`Size{std::numeric_limits<float>::infinity(), ...}`) is the standard WinRT way to
   ask "how wide would this element be if space were unlimited?" — the result is in
   `DesiredSize`. Is this call safe to make on a live element that is already in the
   visual tree and has already been laid out? Are there known failure modes for this
   pattern in an explorer.exe-hosted XAML tree?

3. `LayoutUpdated` fires on every layout pass for the entire tree, not just the element
   it's subscribed on. What is the risk of using `LayoutUpdated` on the title `TextBlock`
   as the trigger to re-check overflow and restart the animation? Specifically: could
   it fire during the animation itself, creating a feedback loop that restarts the scroll
   mid-cycle?

---

### Section 2 — Why a clipping container is required

For a `TranslateTransform` marquee to work, the scrolling text must be clipped to the
column width. Without a clip, the text would scroll visibly outside the widget boundary
into the clock/tray area.

1. The current `StackPanel textCol` has no explicit `Clip` set and no `ClipToBounds`
   equivalent. In WinRT XAML, is there a panel-level clip-to-bounds property, or does
   a wrapping `Border` with `ClipToBounds` need to be introduced?

2. If a `Border(ClipToBounds = true)` wrapper is added around the title `TextBlock`,
   describe what structural change this requires to the existing XAML tree. Specifically:
   can the title TextBlock remain a direct child of the `StackPanel`, or does the
   `StackPanel` also need to be restructured?

3. The star column (col 2) gives the `StackPanel` a measured width equal to available
   space. If a `Border` wrapper is interposed between the `StackPanel` and the
   `TextBlock`, what sizing behavior must the `Border` have to correctly limit the
   TextBlock's rendered width to the column's available width? Should the `Border` be
   `HorizontalAlignment::Stretch` or have an explicit width?

---

### Section 3 — Why DispatcherTimer is problematic in this environment

The first attempted approach used a `DispatcherTimer` at 16 ms.

1. A `DispatcherTimer` in WinRT requires a `CoreDispatcher` or the thread must have a
   `CoreWindow` / message loop. The GSMTC background thread and the poll thread in this
   mod are plain Win32 threads with no XAML dispatcher. The widget is manipulated via
   `Dispatcher().RunAsync()`. Can a `DispatcherTimer` be created and started from a
   `RunAsync` lambda on the UI dispatcher thread of an injected XAML island — and if so,
   does it survive beyond the lambda's scope if stored in a `static` or captured in a
   lambda?

2. If the mod is unloaded (Windhawk calls `Wh_ModUninit`) while a `DispatcherTimer` is
   active, what happens? The timer's tick callback would fire on the UI dispatcher thread
   and attempt to access `g_WidgetRoot` and the title `TextBlock`. Is there a safe
   teardown path, or does the timer need to be explicitly stopped and its handle retained
   to allow cleanup?

3. At 16 ms (≈60 fps), a `DispatcherTimer` callback fires on the UI thread for every
   frame of the scroll. The taskbar UI thread also handles all taskbar rendering, system
   tray updates, app thumbnail previews, etc. Is a 60 fps timer on the taskbar's UI
   thread likely to cause visible jank or scheduling latency on a typical system?

---

### Section 4 — Why Storyboard/DoubleAnimation is the canonical approach, and its constraints

The second attempted approach used `Storyboard` + `DoubleAnimation`.

1. `DoubleAnimation` targeting a `TranslateTransform.X` property is the WinRT XAML
   canonical animation for this use case. The XAML compositor runs animations
   independently of the UI thread once started. Does this apply to
   `explorer.exe`-hosted XAML islands in the same way as a standard UWP app, or are
   there known constraints on compositor-driven animations in injected XAML trees?

2. To target `TranslateTransform.X` with a `DoubleAnimation`, the `TextBlock` needs a
   `RenderTransform` set to a `TranslateTransform` at construction time. The current
   code does not set `RenderTransform` on the title `TextBlock` (read the source to
   confirm). What is the consequence of attaching a `Storyboard` that targets a
   `TranslateTransform` on an element that was not constructed with one — does the
   animation fail silently, throw, or require the transform to be set first?

3. The `Storyboard` needs a `From` value (0) and a `To` value (negative of the text's
   natural overhang width). The `To` value is not known until after layout. Describe
   the correct sequence of operations: when should the `Storyboard` be created, when
   should `To` be set, and when should `Begin()` be called — relative to the layout
   pass that makes `DesiredSize` valid?

4. When the title text changes (new song), the running `Storyboard` must stop, the
   `TranslateTransform.X` must reset to 0, the new text must lay out, and a new
   storyboard must begin with a new `To` value. In the current architecture, title
   updates happen via `ApplyStateToWidget()` → `RunAsync` on the UI dispatcher.
   Describe the sequencing risk: is it safe to `Stop()` + `Begin()` a new storyboard
   from within that same `RunAsync` callback, or is a deferred layout pass required
   between stop and restart?

---

### Section 5 — Windhawk-specific constraints

1. The Windhawk MinGW toolchain used to compile this mod may not ship all WinRT
   XAML animation headers. The `winrt/Windows.UI.Xaml.Media.Animation.h` include is
   present (line 142). List the specific WinRT types that a Storyboard marquee requires:
   `Storyboard`, `DoubleAnimation`, `TranslateTransform`, `PropertyPath` or
   `Storyboard::SetTargetProperty`. Are all of these in the `Windows.UI.Xaml.Media.Animation`
   or `Windows.UI.Xaml.Media` namespace, or do any require additional headers?

2. `Storyboard::SetTargetProperty` takes a `DependencyProperty` or a property path string.
   In C++/WinRT, `TranslateTransform::XProperty()` is the dependency property accessor.
   Is this accessor available as a static method in MinGW-compiled C++/WinRT, or is
   there a known toolchain gap that requires a string-based property path instead?

3. This mod stores all widget references as weak_refs (`weak_ref<Grid> g_WidgetRoot`,
   etc.) to survive widget removal during async operations. A `Storyboard` that targets
   the `TextBlock`'s `TranslateTransform` holds an implicit strong reference to the
   element. If the widget is removed from the XAML tree (e.g., on Explorer restart
   or mod unload), does the animation infrastructure release that reference automatically,
   or must `Storyboard::Stop()` be called explicitly before removing the element from
   the tree?

---

### Section 6 — Assessment and recommendation

Having reviewed the above, provide:

1. **Root cause diagnosis:** In one paragraph, explain why neither of the two previously
   attempted approaches (DispatcherTimer and Storyboard/LayoutUpdated) produced a working
   marquee. Base your answer on the specific structural and API constraints identified in
   Sections 1–5.

2. **Viable approach:** Describe, conceptually and without writing code, the complete
   sequence of steps required to implement a working marquee in this specific XAML
   structure. Include: what XAML tree change is needed, what API measures overflow, what
   drives the animation, when it starts/stops/resets, and how cleanup works on unload.

3. **Complexity rating:** Rate this feature on a scale of Low / Medium / High / Very High
   relative to the other features in this mod (cover art loading, Chameleon background,
   multi-session). Justify your rating.

4. **Risk flags:** List any specific risks or unknowns that would need to be resolved
   or prototyped before committing to an implementation.

---

## Deliverable Format

Return a structured report with one section per research section above. Each section
should have a **Findings** subsection (what you found reading the source + WinRT docs)
and a **Conclusion** subsection (the direct answer to the research questions).

Do not write implementation code. Do not edit any files. Do not propose features beyond
the scope of the marquee scroll question.
