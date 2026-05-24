# OL-9: Phase 3 — Background Theming Handoff

**Date:** 2026-05-23  
**Target file:** `native-taskbar-media-controller.wh.cpp` (1861 lines at handoff time)  
**Branch:** `main` (HEAD `01970c9`, tagged `v0.2.0-beta.5`)  
**Target version:** `0.2.0-beta.6`

---

## Context

This is a native XAML Windhawk mod injected into `explorer.exe`. It renders a media
controller widget directly inside the taskbar's own XAML tree — no Win32 overlay
window. All rendering is WinRT XAML. The widget root is a `Grid` named
`kWidgetRootName`; it is added as a child of `Grid#RootGrid` under
`Taskbar.TaskbarFrame`.

**Read these files first:**
1. `native-taskbar-media-controller.wh.cpp` — full source
2. `.raiden/state/CURRENT_STATE.md` — what is already shipped
3. `.raiden/state/OPEN_LOOPS.md` — OL-9 section for the agreed scope

---

## What Is Already Done (Do Not Redo)

**Acrylic background is already implemented and shipping** in `BuildWidget()` at lines
580–590. The try/catch fallback is correct and working. The acrylic gate mentioned in
OL-9 has been implicitly passed — it was shipped in `v0.2.0-beta.2` and confirmed
working inside Explorer's injected XAML tree.

The current acrylic code is **unconditional** — it always applies. OL-9's job is
to convert it into a user-selectable `BackgroundStyle` setting.

---

## Scope

Add a `BackgroundStyle` enum setting with three values:

| Value | Behaviour |
|---|---|
| `None` | No background brush (transparent root Grid) |
| `Acrylic` | Current `AcrylicBrush(HostBackdrop)` with dark-fallback — existing code, just gated |
| `Chameleon` | `LinearGradientBrush` derived from album art via 64-bucket dominant-color quantization; updates on each art load; transparent when no art is present |

**Default value:** `Acrylic` (preserves existing beta.5 behaviour for existing users).

---

## Step 1 — Settings Block

### 1a. Windhawk YAML settings block (lines 55–75)

Append below the existing `MarqueeTitle` entry inside `// ==WindhawkModSettings==`:

```yaml
- BackgroundStyle: 1
  $name: Background style
  $description: "0 = None (transparent), 1 = Acrylic (frosted glass), 2 = Chameleon (album art gradient)"
  $options:
  - 0: None
  - 1: Acrylic
  - 2: Chameleon
```

Also add a row to the Settings table in the Readme block:

```
| Background style | Acrylic | Transparent / Acrylic (frosted glass) / Chameleon (album art gradient) |
```

### 1b. `ModSettings` struct (lines 158–167)

Add field:

```cpp
int backgroundStyle = 1;  // 0=None 1=Acrylic 2=Chameleon
```

### 1c. `LoadSettings()` (lines 187–200)

Add after the existing reads:

```cpp
g_Settings.backgroundStyle = Wh_GetIntSetting(L"BackgroundStyle");
if (g_Settings.backgroundStyle < 0 || g_Settings.backgroundStyle > 2)
    g_Settings.backgroundStyle = 1;
```

---

## Step 2 — Gate Acrylic in `BuildWidget()`

Replace the unconditional acrylic block at lines 580–590:

```cpp
// Before (unconditional):
try {
    AcrylicBrush acrylic;
    acrylic.BackgroundSource(AcrylicBackgroundSource::HostBackdrop);
    acrylic.TintColor(ColorHelper::FromArgb(0xFF, 0x1A, 0x1A, 0x1A));
    acrylic.TintOpacity(0.6);
    root.Background(acrylic);
} catch (...) {
    root.Background(MakeBrush(0xCC, 0x1A, 0x1A, 0x1A));
}
```

```cpp
// After (gated):
if (g_Settings.backgroundStyle == 1) {  // Acrylic
    try {
        AcrylicBrush acrylic;
        acrylic.BackgroundSource(AcrylicBackgroundSource::HostBackdrop);
        acrylic.TintColor(ColorHelper::FromArgb(0xFF, 0x1A, 0x1A, 0x1A));
        acrylic.TintOpacity(0.6);
        root.Background(acrylic);
    } catch (...) {
        root.Background(MakeBrush(0xCC, 0x1A, 0x1A, 0x1A));
    }
}
// backgroundStyle == 0 (None): root.Background stays nullptr (transparent)
// backgroundStyle == 2 (Chameleon): set in ApplyStateToWidget after art loads
```

---

## Step 3 — Chameleon: Dominant Color Quantization

Chameleon derives up to two dominant colors from the album art using a 64-bucket
histogram, then applies them as a `LinearGradientBrush` on the widget root background.

### 3a. Required namespace

Add to the `using namespace` block:

```cpp
using namespace Windows::Graphics::Imaging;
```

(`BitmapDecoder`, `SoftwareBitmap`, `BitmapPixelFormat`, `BitmapAlphaMode` live here.)

### 3b. Helper: `ComputeDominantColors`

Add this free function before `ApplyStateToWidget`:

```cpp
// Returns up to two dominant RGBA colors from album art via 64-bucket quantization.
// Each channel is reduced to 2 bits (6 bits discarded), giving 4^3 = 64 buckets.
// Returns {primary, secondary} — secondary equals primary when only one bucket dominates.
static std::pair<Windows::UI::Color, Windows::UI::Color>
ComputeDominantColors(SoftwareBitmap const& bmp) {
    auto buf = bmp.LockBuffer(BitmapBufferAccessMode::Read);
    auto ref  = buf.CreateReference();
    auto desc = buf.GetPlaneDescription(0);

    uint8_t const* px   = reinterpret_cast<uint8_t const*>(
                              ref.as<::Windows::Foundation::IMemoryBufferByteAccess>()
                                 ->GetBuffer(nullptr));
    int stride = desc.Stride;
    int width  = desc.Width;
    int height = desc.Height;

    // 64-bucket histogram: (R>>6)<<4 | (G>>6)<<2 | (B>>6)
    uint32_t counts[64]{};
    uint32_t sumR[64]{}, sumG[64]{}, sumB[64]{};

    for (int y = 0; y < height; ++y) {
        uint8_t const* row = px + y * stride;
        for (int x = 0; x < width * 4; x += 4) {  // Bgra8
            uint8_t b = row[x], g = row[x+1], r = row[x+2];
            int bucket = ((r>>6)<<4) | ((g>>6)<<2) | (b>>6);
            counts[bucket]++;
            sumR[bucket] += r;
            sumG[bucket] += g;
            sumB[bucket] += b;
        }
    }

    // Find top two buckets by count.
    int top1 = 0, top2 = 0;
    for (int i = 1; i < 64; ++i) {
        if (counts[i] > counts[top1]) { top2 = top1; top1 = i; }
        else if (i != top1 && counts[i] > counts[top2]) { top2 = i; }
    }

    auto avgColor = [&](int bucket) -> Windows::UI::Color {
        uint32_t c = counts[bucket];
        if (c == 0) return ColorHelper::FromArgb(0xFF, 0x1A, 0x1A, 0x1A);
        return ColorHelper::FromArgb(0xFF,
            (uint8_t)(sumR[bucket]/c),
            (uint8_t)(sumG[bucket]/c),
            (uint8_t)(sumB[bucket]/c));
    };

    return { avgColor(top1), counts[top2] > 0 ? avgColor(top2) : avgColor(top1) };
}
```

**Notes:**
- `SoftwareBitmap` pixel format must be `Bgra8`. Enforce this in Step 3c.
- `IMemoryBufferByteAccess` requires `#include <windows.graphics.imaging.interop.h>` or
  the equivalent WinRT COM projection. If unavailable, use `ref.data()` via the
  `IMemoryBufferByteAccess` COM QI pattern already common in WinRT samples.
- The 64-bucket approach is fast, allocation-free, and good enough for gradient
  purposes. A more accurate median-cut is explicitly out of scope.

### 3c. Decode art pixels in the existing `fire_and_forget` art loader

The art loader in `ApplyStateToWidget` (lines 1015–1034) currently:
1. Opens a stream via `ref.OpenReadAsync()`
2. Creates `BitmapImage bitmap`
3. Calls `bitmap.SetSourceAsync(stream)` — this consumes the stream position

For Chameleon, open a **second** stream from the same `thumbnailRef` for pixel access.
`IRandomAccessStreamReference` supports multiple `OpenReadAsync()` calls.

Inside the `fire_and_forget` lambda, after `co_await bitmap.SetSourceAsync(stream)`:

```cpp
// Chameleon: derive dominant colors from a second stream open on the same ref.
if (g_Settings.backgroundStyle == 2) {
    try {
        auto stream2   = co_await ref.OpenReadAsync();
        auto decoder   = co_await BitmapDecoder::CreateAsync(stream2);
        auto softBmp   = co_await decoder.GetSoftwareBitmapAsync(
                             BitmapPixelFormat::Bgra8,
                             BitmapAlphaMode::Ignore);
        auto [c1, c2]  = ComputeDominantColors(softBmp);

        // Back to UI thread to set background brush.
        auto weakRoot  = make_weak(el.Parent().try_as<FrameworkElement>()
                            .try_as<Grid>());  // el.Parent() = layout Grid; its parent = root Grid
        // Safer: pass weakRoot separately. See note below.
        co_await el.Dispatcher().RunAsync(
            Windows::UI::Core::CoreDispatcherPriority::Normal,
            [weakRoot, c1, c2]() {
                auto root = weakRoot.get();
                if (!root || g_Unloading.load()) return;
                LinearGradientBrush grad;
                grad.StartPoint({0.0, 0.0});
                grad.EndPoint({1.0, 0.0});
                GradientStop s1;
                s1.Color(c1);
                s1.Offset(0.0);
                GradientStop s2;
                s2.Color(c2);
                s2.Offset(1.0);
                grad.GradientStops().Append(s1);
                grad.GradientStops().Append(s2);
                root.Background(grad);
            });
    } WH_CATCH(L"ApplyStateToWidget/ChameleonColors")
}
```

**Parent traversal note:** `el` is the `Image` (album art). Its parent in the XAML
tree is `layout` (the inner `Grid`), whose parent is `root` (the widget root `Grid`
named `kWidgetRootName`). The simplest way to get `root` is to capture a
`weak_ref<Grid>` to it before the lambda is entered. Add a
`FindByName<Grid>(widget, kWidgetRootName)` call before the art loader block and
capture `make_weak(rootGrid)` alongside `weakArt`.

### 3d. Clear Chameleon background when no art

In the no-art branch of the art loader (`!hasMedia || !thumbnailRef`), and also in
the "no media" collapse at the end of `ApplyStateToWidget`:

```cpp
if (g_Settings.backgroundStyle == 2) {
    if (auto root = FindByName<Grid>(widget, kWidgetRootName))
        root.Background(nullptr);  // transparent when no art
}
```

### 3e. Chameleon adaptive text color

The existing SC-UI-2 logic (lines 970–979) derives text color from
`IsSystemLightTheme()`. For Chameleon, the dominant color brightness should drive
this instead — so text is readable against the gradient.

Compute luma of `c1` after calling `ComputeDominantColors`. Pass it out or compute
inline. A simple BT.601 luma threshold:

```cpp
// luma in [0, 255]; >128 = light background → dark text
uint8_t luma = (uint8_t)(0.299f*c1.R + 0.587f*c1.G + 0.114f*c1.B);
bool chameleonLightBg = (luma > 128);
```

Store this derived value and pass it to the same `fgHi` path already in
`ApplyStateToWidget`. The cleanest approach is to compute it in the
`fire_and_forget` lambda (where `c1` is available) and call a separate
`RefreshWidgetTextColor(bool lightBg)` helper on the UI thread — or simply
call `RefreshWidgetUI()` again after setting the background, which will re-read
whatever state is current. The second call will be a no-op for everything except
text color.

**Alternative (simpler):** Store `g_ChameleonLightBg` as a global `std::atomic<bool>`
(default `false`). Set it in the fire_and_forget after computing luma. The next
`ApplyStateToWidget` call will pick it up. This is slightly stale (text color lags
one UI cycle behind the gradient) but imperceptible in practice.

---

## Step 4 — Settings Changed Handling

`Wh_ModSettingsChanged` calls `LoadSettings()` then `RefreshWidgetUI()`. Since
`BuildWidget()` only runs once at injection time, a `backgroundStyle` change after
injection cannot rebuild the widget. Two options:

**Option A (recommended):** Apply the background change in `ApplyStateToWidget` rather
than only in `BuildWidget()`. On each `ApplyStateToWidget` call, check
`g_Settings.backgroundStyle`:
- `0` (None): set `root.Background(nullptr)`
- `1` (Acrylic): set the `AcrylicBrush` (with try/catch) if not already an `AcrylicBrush`
- `2` (Chameleon): handled by the art loader fire_and_forget; set `nullptr` here as
  the initial state; the loader will replace it once art arrives

Move the acrylic code from `BuildWidget()` entirely into `ApplyStateToWidget`, and
remove it from `BuildWidget()`. `BuildWidget()` sets no background.

**Option B:** On `Wh_ModSettingsChanged`, call `ReinjectWidget()` / `DetachWidget()`
+ `InjectWidgetInto()`. This is heavier but guarantees a clean state. Not recommended
unless Option A causes issues.

---

## What NOT to Do

- **Do not use GDI+ or `Bitmap::LockBits`** — the XAML art pipeline is pure WinRT.
- **Do not attempt median-cut, k-means, or color space conversion to HSL/LAB** —
  64-bucket quantization in RGB is the agreed approach; more accuracy is out of scope.
- **Do not add blurred background art** — that is a separate concept (SC-UI-1). Not in scope.
- **Do not set any XAML element property off the UI dispatcher thread** — all writes
  after co_awaits must go through `Dispatcher().RunAsync(...)`.
- **Do not store `SoftwareBitmap` or `BitmapDecoder` in global state** — they are
  large and only needed transiently during the art load.
- **Do not block the GSMTC thread or `g_MediaMutex` for color computation** —
  `ComputeDominantColors` runs inside the `fire_and_forget` on the thread pool.

---

## Testing Checklist

- [ ] `BackgroundStyle = 0` (None): widget root is transparent, no visible background
- [ ] `BackgroundStyle = 1` (Acrylic): frosted-glass effect visible, same as pre-beta.6
- [ ] `BackgroundStyle = 2` (Chameleon), Spotify playing: gradient background matches
  album art colors
- [ ] Chameleon: track change → gradient updates to new art
- [ ] Chameleon: session with no art (Libby) → widget background is transparent
- [ ] Chameleon: text color adapts to light vs dark gradient (readable against background)
- [ ] Acrylic → Chameleon → None setting change at runtime: background updates without
  mod reload (or clearly documented that mod reload is required if Option B chosen)
- [ ] Mod unload while Chameleon color compute is in flight: no crash
  (`g_Unloading` guard + `weak_ref` on root)
- [ ] No regression in marquee scroll, progress bar, or double-tap focus

---

## Commit Convention

```
feat: OL-9 — BackgroundStyle setting (None / Acrylic / Chameleon) (v0.2.0-beta.6)

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
```
