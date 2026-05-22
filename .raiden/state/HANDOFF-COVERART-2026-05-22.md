# Cover Art — General Implementation Handoff

**Date:** 2026-05-22  
**Target file:** `native-taskbar-media-controller.wh.cpp`  
**Branch:** `main` (current HEAD)

---

## Context

This is a native XAML Windhawk mod injected into `explorer.exe`. It renders a
media controller widget inside the taskbar's own XAML tree — no Win32 overlay
window, no GDI+. All rendering is WinRT XAML.

**Do not reference the forks for cover art patterns.** Every fork (Uiisland,
memeri121, GR0UD) uses GDI+ `Bitmap` / `Graphics::DrawImage` to paint into a
Win32 window. That pipeline is entirely irrelevant here. The correct approach is
WinRT `BitmapImage` + XAML `Image` control.

---

## Read These Files First

1. `native-taskbar-media-controller.wh.cpp` — full file (~1065 lines)
2. `.raiden/state/CURRENT_STATE.md` — architecture overview
3. `.raiden/state/HANDOFF-LIBBY-2026-05-22.md` — Libby null-thumbnail caveat
   (Libby does not populate `props.Thumbnail()`; this is a confirmed known
   limitation, not a bug to investigate)

---

## What Exists Today

**`MediaState` struct (line ~154):**
```cpp
struct MediaState {
    std::wstring title;
    std::wstring artist;
    std::wstring sessionId;
    bool isPlaying = false;
    double playbackRate = 1.0;
    bool canSkipForward  = false;
    bool canSkipBackward = false;
    int64_t positionMs = 0;
    int64_t durationMs = 0;
    GlobalSystemMediaTransportControlsSession session{ nullptr };
    event_token propsChangedToken{};
    event_token playbackChangedToken{};
};
```
No thumbnail field exists.

**`BuildWidget()` layout (line ~274):** horizontal `StackPanel` containing:
1. Session count chip (`TextBlock`, collapsed by default)
2. Text column (`StackPanel` with title + artist `TextBlock`s)
3. SkipBack button (`«`)
4. Play/pause button
5. Next/SkipFwd button (`»`)

**`UpdateOneSessionAsync`:** `fire_and_forget` coroutine. Already `co_await`s
`session.TryGetMediaPropertiesAsync()` — the `props` object is in hand.

**`ApplyStateToWidget`:** synchronous function called from a
`Dispatcher().RunAsync` lambda — it runs on the UI thread. Safe to set any
XAML element property here.

---

## Required Namespace

Add to the `using namespace` block at the top of the file:
```cpp
using namespace Windows::UI::Xaml::Media::Imaging;
```
(`BitmapImage` lives here. `Image` control is already in
`Windows::UI::Xaml::Controls`.)

---

## Implementation

### Step 1 — Add thumbnail state to `MediaState`

```cpp
IRandomAccessStreamReference thumbnailRef{ nullptr };  // null = no art (Libby, etc.)
uint32_t thumbnailVersion = 0;                         // incremented each time art changes
```

`IRandomAccessStreamReference` is a lightweight COM interface pointer. Safe to
store in the struct and share across threads under `g_MediaMutex`. It is the
stream *reference* (not an opened stream) — cheap to hold, opened on demand.

### Step 2 — Fetch thumbnail in `UpdateOneSessionAsync`

Inside the existing `try` block that already reads `props`, after the existing
`AlbumTitle`/`AlbumArtist` fallback logic:

```cpp
IRandomAccessStreamReference newThumbRef{ nullptr };
try {
    newThumbRef = props.Thumbnail();  // null for Libby and apps with no art
} WH_CATCH(L"UpdateOneSessionAsync/Thumbnail")
```

In the change-detection / write-back block (under `g_MediaMutex`):

```cpp
// Thumbnail: treat any change in Thumbnail() as new art (version bump).
// IRandomAccessStreamReference identity comparison is not reliable across
// MediaPropertiesChanged callbacks; use a simple always-update on title change.
// Since we only reach this block when title/artist/playing actually changed,
// always bumping version here is correct and not noisy.
bool artChanged = (m.thumbnailRef != newThumbRef);  // COM identity check
if (artChanged) {
    m.thumbnailRef    = newThumbRef;
    m.thumbnailVersion++;
}
```

If COM identity comparison proves unreliable in practice (e.g. the same art
re-wrapped in a new object), fall back to always incrementing `thumbnailVersion`
on every `MediaPropertiesChanged` callback — this is safe because
`ApplyStateToWidget` will just re-load the same image.

### Step 3 — Add `Image` element to `BuildWidget()`

Insert as the **first child** of `layout` (before the session count chip), so
art appears at the left edge of the widget.

```cpp
// Named constant (add near other kXxx constants at top of file)
constexpr std::wstring_view kAlbumArtName = L"NowPlayingAlbumArt";

// In BuildWidget():
Image albumArt;
albumArt.Name(kAlbumArtName);
albumArt.Width((double)g_Settings.panelHeight);
albumArt.Height((double)g_Settings.panelHeight);
albumArt.Stretch(Stretch::UniformToFill);   // crop to fill square; no letterbox
albumArt.VerticalAlignment(VerticalAlignment::Stretch);
albumArt.Margin(ThicknessHelper::FromLengths(0, 0, 6, 0));  // 6px gap before text
albumArt.Visibility(Visibility::Collapsed);  // hidden until art is loaded
layout.Children().Insert(0, albumArt);       // first child
```

**Size rationale:** `panelHeight` × `panelHeight` makes it a square that fills
the widget height. Default `panelHeight` is 40px — comfortable for taskbar use.

### Step 4 — Load and display art in `ApplyStateToWidget`

`ApplyStateToWidget` already runs on the UI dispatcher. After the existing
state reads (title, artist, isPlaying, etc.), add:

```cpp
uint32_t thumbnailVersion = 0;
IRandomAccessStreamReference thumbnailRef{ nullptr };
{
    // (already inside the g_MediaMutex lock that reads other fields)
    thumbnailVersion = m.thumbnailVersion;
    thumbnailRef     = m.thumbnailRef;
}
```

Then, after setting the existing TextBlock/button states, add the art loader.
Because opening a stream is async, launch a `fire_and_forget` from inside
`ApplyStateToWidget`, capturing what we need by value:

```cpp
auto artEl = FindByName<Image>(widget, kAlbumArtName);
if (artEl) {
    if (!thumbnailRef) {
        // No art (Libby, or app with no thumbnail) — collapse and clear.
        artEl.Source(nullptr);
        artEl.Visibility(Visibility::Collapsed);
    } else {
        // Art available — load async, then reveal.
        // Capture artEl as weak_ref to avoid holding a strong ref across
        // the co_await if the widget is removed.
        auto weakArt = make_weak(artEl);
        auto ref     = thumbnailRef;         // copy for lambda capture
        auto version = thumbnailVersion;
        [](weak_ref<Image> weakEl, IRandomAccessStreamReference ref,
           uint32_t version) -> winrt::fire_and_forget {
            g_AsyncTasks++;
            struct Guard { ~Guard() { g_AsyncTasks--; } } g;

            try {
                auto stream = co_await ref.OpenReadAsync();
                BitmapImage bitmap;
                co_await bitmap.SetSourceAsync(stream);
                // Resume back on the UI dispatcher to set Source.
                auto el = weakEl.get();
                if (!el || g_Unloading.load()) co_return;
                el.Source(bitmap);
                el.Visibility(Visibility::Visible);
            } WH_CATCH(L"ApplyStateToWidget/LoadArt")
        }(weakArt, ref, version);
    }
}
```

**Threading note:** `co_await ref.OpenReadAsync()` and
`co_await bitmap.SetSourceAsync(stream)` resume on the thread pool. After both
awaits, marshal back to the UI dispatcher before calling `el.Source(bitmap)`.
Use the same dispatcher pattern already in `RefreshWidgetUI`:

```cpp
// After co_await bitmap.SetSourceAsync(stream):
auto el = weakEl.get();
if (!el || g_Unloading.load()) co_return;
// el.Dispatcher() gives us back the UI thread:
co_await el.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [el, bitmap]() {
    el.Source(bitmap);
    el.Visibility(Visibility::Visible);
});
```

This is the safe pattern. Do not set `el.Source()` directly after an await
without first dispatching back to the UI thread.

### Step 5 — Clear art on session change / no media

In `ApplyStateToWidget`, when `!hasMedia`:
```cpp
if (artEl) {
    artEl.Source(nullptr);
    artEl.Visibility(Visibility::Collapsed);
}
```
This ensures the art element doesn't show stale art from a previous session.

---

## What NOT to Do

- **No GDI+, no `Bitmap`, no `Graphics::DrawImage`** — those are for the Win32
  overlay forks. Irrelevant here.
- **No `OpenReadAsync().get()`** — blocking call on a coroutine thread. Use
  `co_await` throughout.
- **Do not set `Image::Source` after a `co_await` without dispatching back to
  the UI thread first.** This will throw or silently fail.
- **Do not add blurred background art** — that is SC-UI-1 scope and is not part
  of this handoff. Plain square thumbnail only.
- **Do not store `BitmapImage` in `MediaState`** — `BitmapImage` is a UI
  object; storing it in a struct accessed from multiple threads under a mutex
  is unsafe. Store only the `IRandomAccessStreamReference` (inert COM pointer).

---

## Libby Handling

Libby returns null from `props.Thumbnail()`. The null-check (`if (!thumbnailRef)`)
in Step 4 handles this — the Image element is collapsed and `Source` is cleared.
No special AUMID detection needed.

---

## Testing Checklist

- [ ] Spotify playing: album art appears, correct square crop
- [ ] Track change on Spotify: art updates to new track's image
- [ ] Libby playing: no art shown, Image element collapsed, no error in logs
- [ ] App with no thumbnail: same as Libby — graceful collapse
- [ ] Session cycle (two sessions): art switches correctly when cycling
- [ ] Mod unload while art is loading: no crash (g_AsyncTasks drain + weak_ref guard)
- [ ] Widget `panelWidth` too narrow for art + text: text column truncates
  correctly with ellipsis, art is not clipped

---

## Commit Convention

```
feat: general cover art via XAML BitmapImage

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
```
