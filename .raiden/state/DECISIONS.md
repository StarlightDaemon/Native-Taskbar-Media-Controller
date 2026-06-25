# Decisions

## D-1: Widget Deployment Model — Native XAML Injection

The widget is inserted directly into the taskbar's own XAML tree (`Grid#RootGrid`
under `Taskbar.TaskbarFrame`). Overlay window, `SetLayeredWindowAttributes`, and a
GDI paint loop were considered and rejected.

**Rationale:** native injection inherits z-ordering, auto-hide behavior, and DPI
scaling from the taskbar without any custom plumbing. An overlay window would
require independent z-order management and DPI tracking that the taskbar already
provides for free.

---

## D-2: Flyout Implementation — Win32 WS_POPUP on Dedicated Thread

The hover flyout is a Win32 `WS_POPUP` HWND driven by a dedicated thread
(`g_FlyoutThread`) with GDI paint (`HALFTONE` StretchBlt for album art).

**Probed and rejected:** WUX `Popup` inside the XAML Island compositor target. The
`IsOpen` property fires correctly but the compositor produces no visible output —
`Popup` is non-functional within a XAML Island compositor target. Win32 popup was
adopted as the only viable path.

---

## D-3: Background Theming — Acrylic Only

`AcrylicBrush` (HostBackdrop) is the sole background mode. Chameleon (64-bucket
dominant-color quantization → `LinearGradientBrush`) and BlurredArt were shipped
in v1.3.x and removed in v1.4.x.

**Rationale:** complexity vs. value tradeoff. Chameleon was the primary consumer of
the `ComputeDominantColors` helper; removing both simplified the codebase without
meaningful loss of user-facing capability. Acrylic was the default mode and the one
users actually relied on.

---

## D-4: SC-SP-1 (Interactive Seek Bar) — Will Not Be Implemented

Explicitly out of scope. Do not revisit.

---

## D-5: SC-0X-1 (Display-Only Mode) — Will Not Be Implemented

Explicitly out of scope. Do not revisit.

---

## D-6: Single-File Architecture — No External Build System

The entire mod is one `.wh.cpp` translation unit. There is no CMake, no vcpkg, no
lockfile, and no package manager. Windhawk compiles the file on-device at load time.

**Rationale:** keeps the mod self-contained and eliminates install dependencies
beyond Windhawk itself. **Consequence:** SCA (software composition analysis) is
structurally inapplicable — there are no declared dependencies to scan.

---

## D-7: Thread Model — Dedicated Threads, XAML Always on UI Thread

Three dedicated threads handle work outside the UI thread:

- `g_GsmtcThread` — GSMTC session management and media event callbacks
- `g_FlyoutThread` — flyout Win32 message loop and GDI paint
- `g_PollThread` — fullscreen-state polling (`SHQueryUserNotificationState`, 1 s interval)

All XAML operations are marshaled to the UI thread via the dispatcher (`RunAsync` /
`TryEnqueueAsync`). This is a hard constraint enforced by the XAML STA threading
contract, not a stylistic preference — calling XAML APIs off the UI thread produces
undefined behavior or silent failures.
