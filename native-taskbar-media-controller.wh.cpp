// ==WindhawkMod==
// @id              native-taskbar-media-controller
// @name            Native Taskbar Media Controller
// @description     Native XAML-injected media controller in the Windows 11 taskbar — shows now-playing info with playback controls.
// @version         1.5.0
// @author          StarlightDaemon
// @github          https://github.com/StarlightDaemon/Native-Taskbar-Media-Controller
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lole32 -loleaut32 -lruntimeobject -luser32 -lwindowsapp -lshell32 -lgdi32 -ldwmapi -DWINVER=0x0A00 -Wl,--undefined=__imp_FindWindowW -Wl,--undefined=__imp_FindWindowExW -Wl,--undefined=__imp_PostMessageW -Wl,--undefined=__imp_GetClientRect
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Native Taskbar Media Controller

Inserts the widget directly into the taskbar's own XAML tree (`Grid#RootGrid`
under `Taskbar.TaskbarFrame`). No overlay window, no `SetLayeredWindowAttributes`,
no GDI painting loop. The widget inherits correct z-ordering, auto-hide handling,
and DPI scaling from the taskbar itself.

## Features

- **Now playing** — title and artist from any GSMTC-compatible source: Spotify,
  YouTube Music, Windows Media Player, browsers, audiobook apps, and anything
  else that registers a media session
- **Playback controls** — play/pause toggle, skip-next, and skip-back; skip
  buttons are always shown but dimmed when not supported by the source
- **Hover flyout** — hovering over the widget shows an expanded panel with
  full-width album art, title, and artist; follows the system dark/light theme
- **Multi-session** — when multiple media apps are active simultaneously, a
  session count chip appears in the top-right corner; tap it to cycle through
  sessions
- **Audiobook mode** — sessions longer than one hour (or with chapter keywords)
  are treated as audiobooks: skip buttons navigate chapters and playback speed
  is shown next to the title

Also includes: scrolling marquee title, inline album art, track progress bar
with position/duration timestamp, smooth progress interpolation between SMTC
ticks, text crossfade on track changes, widget fade animations, acrylic
frosted-glass background, adaptive text color, fullscreen auto-hide, live
repositioning as tray icons change, double-click to focus the source app, and
middle-click to stop the session.

## Compatibility

Works with any app that registers a Windows GSMTC session. Native apps get
full support; browser sessions have protocol-level limitations that apply
regardless of which browser is used.

| Source | Timeline | Notes |
|---|---|---|
| **Native apps** — Spotify, Apple Music, Amazon Music, Tidal, Deezer, VLC, Windows Media Player, MusicBee, foobar2000, and most other media players | ✓ | Full support — title, artist, album art, and playback controls |
| **Chromium browsers** — Chrome, Edge, Brave, Opera, Vivaldi, Arc, Thorium | — | One SMTC session per browser process; all open tabs share it |
| **Firefox** | — | Full title and artist; no timeline data (Mozilla bug 1689538) |
| **Libby** | — | Runs as a Chrome extension; the mod substitutes an embedded icon since Libby does not expose cover art via SMTC |
| **Audiobookshelf** | ✓ | Chapter navigation active for sessions over one hour, or when the title contains the word "Chapter" |
| **Audible Cloud Player** | — | Browser session via Chromium; treated the same as any other browser tab |

## Requirements

- Windows 11 (22H2 or later recommended)
- [Windhawk](https://windhawk.net) mod loader
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- WidgetPosition: Right
  $name: Widget position
  $description: "Where on the taskbar the widget appears."
  $options:
  - Right: Right — next to clock & tray
  - Left: Left — taskbar far left
  - Center: Center — middle of taskbar
- OffsetX: 8
  $name: Position offset (px)
  $description: "Fine-tune placement. Right: gap from the system tray. Left: gap from the left edge. Center: nudge from center (positive = shift right)."
- PanelWidth: 300
  $name: Widget width (px)
- PanelHeight: 40
  $name: Widget height (px)
- FontSize: 11
  $name: Font size
- HideFullscreen: true
  $name: Hide when fullscreen
- ShowProgress: true
  $name: Show track progress bar and timestamp
  $description: Enables the slim progress bar at the bottom of the widget and the position/duration timestamp. Hidden automatically when the media source does not expose timeline data.
- AdaptiveTextColor: true
  $name: Adaptive text color
  $description: Switches text to near-black in Windows light mode, white in dark mode.
- MarqueeScroll: true
  $name: Scroll long titles
  $description: Marquee-scroll title text that is wider than the available space
- FlyoutTransparent: false
  $name: Flyout transparency
  $description: "On: hover flyout is slightly transparent (92% opaque). Off: flyout uses a solid background that matches the native Windows theme color."
*/
// ==/WindhawkModSettings==

#include <windhawk_api.h>
#include <windhawk_utils.h>

#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <propsys.h>
// PKEY_AppUserModel_ID is declared extern in propkey.h but not exported by the
// MinGW/lld propsys stub. Define the key locally using its well-known GUID/PID.
static const PROPERTYKEY kPKEY_AppUserModel_ID = {
    { 0x9F4C2855, 0x9F79, 0x4B39, { 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3 } }, 5
};


// winbase.h defines GetCurrentTime() as a macro wrapping GetTickCount().
// winrt XAML headers declare a virtual GetCurrentTime(int64_t*) method.
// Undefine the macro before pulling in WinRT to avoid the collision.
#undef GetCurrentTime

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <string>
#include <thread>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Animation.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Graphics.Imaging.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Media::Control;
using namespace Windows::UI;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Automation;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Animation;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::Graphics::Imaging;

// WH_CATCH logs hresult, std::exception, and unknown exceptions with a context label.
// Usage: try { ... } WH_CATCH(L"context")
#define WH_CATCH(ctx)                                                          \
    catch (winrt::hresult_error const& e) {                                    \
        Wh_Log(L"[" ctx L"] hresult 0x%08X: %s", (unsigned)e.code().value,    \
               e.message().c_str());                                           \
    }                                                                          \
    catch (std::exception const& e) {                                          \
        Wh_Log(L"[" ctx L"] std::exception (see debug log)"); (void)e;         \
    }                                                                          \
    catch (...) {                                                              \
        Wh_Log(L"[" ctx L"] unknown exception");                               \
    }

// WH_TRY_OR evaluates expr, returning fallback on any exception.
// Usage: auto s = WH_TRY_OR(props.Title().c_str(), L"Unknown");
#define WH_TRY_OR(expr, fallback) \
    [&]() noexcept {              \
        try {                     \
            return (expr);        \
        } catch (...) {           \
            return (fallback);    \
        }                         \
    }()

// ---------- Settings ----------
enum class WidgetPosition { Right, Left, Center };

struct ModSettings {
    int panelWidth = 300;
    int panelHeight = 40;
    int fontSize = 11;
    int offsetX = 8;
    bool hideFullscreen = true;
    bool showProgress = true;
    bool adaptiveTextColor = true;
    bool marqueeScroll = true;
    bool flyoutTransparent = false;
    WidgetPosition widgetPosition = WidgetPosition::Right;
} g_Settings;

static void LoadSettings() {
    g_Settings.panelWidth   = Wh_GetIntSetting(L"PanelWidth");
    g_Settings.panelHeight  = Wh_GetIntSetting(L"PanelHeight");
    g_Settings.fontSize     = Wh_GetIntSetting(L"FontSize");
    g_Settings.offsetX      = Wh_GetIntSetting(L"OffsetX");
    g_Settings.hideFullscreen    = Wh_GetIntSetting(L"HideFullscreen") != 0;
    g_Settings.showProgress      = Wh_GetIntSetting(L"ShowProgress") != 0;
    g_Settings.adaptiveTextColor  = Wh_GetIntSetting(L"AdaptiveTextColor") != 0;
    g_Settings.marqueeScroll      = Wh_GetIntSetting(L"MarqueeScroll") != 0;
    g_Settings.flyoutTransparent  = Wh_GetIntSetting(L"FlyoutTransparent") != 0;
    if (g_Settings.panelWidth   <= 0) g_Settings.panelWidth = 300;
    if (g_Settings.panelHeight  <= 0) g_Settings.panelHeight = 40;
    if (g_Settings.fontSize     <= 0) g_Settings.fontSize = 11;
    if (g_Settings.offsetX      <  0) g_Settings.offsetX = 8;
    PCWSTR pos = Wh_GetStringSetting(L"WidgetPosition");
    if (wcscmp(pos, L"Left") == 0)
        g_Settings.widgetPosition = WidgetPosition::Left;
    else if (wcscmp(pos, L"Center") == 0)
        g_Settings.widgetPosition = WidgetPosition::Center;
    else
        g_Settings.widgetPosition = WidgetPosition::Right;
    Wh_FreeStringSetting(pos);
}

// ---------- GSMTC multi-session state ----------
static constexpr int MAX_SESSIONS = 10;

// Libby app icon (libbyapp.com/icons/libby-icon-android-192.png), 6796 bytes
static const uint8_t kLibbyIconPng[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0xC0, 0x08, 0x06, 0x00, 0x00, 0x00, 0x52, 0xDC, 0x6C,
    0x07, 0x00, 0x00, 0x1A, 0x53, 0x49, 0x44, 0x41, 0x54, 0x78, 0x01, 0xEC, 0x9D, 0x05, 0x8C, 0xE3,
    0x46, 0x18, 0x85, 0xBB, 0x7C, 0x0C, 0xD9, 0x24, 0x8E, 0x27, 0x29, 0xAD, 0x72, 0x7C, 0x61, 0x7B,
    0x8F, 0x99, 0x99, 0x21, 0x9C, 0x38, 0x27, 0x55, 0x74, 0x27, 0xAA, 0xA8, 0x15, 0x94, 0x99, 0x99,
    0x99, 0x99, 0x59, 0x54, 0x12, 0x43, 0x99, 0x99, 0x99, 0x19, 0xFD, 0x7C, 0xCC, 0x1B, 0x70, 0x62,
    0x3B, 0x6F, 0xA4, 0xAF, 0xB8, 0x60, 0xBF, 0xFF, 0x1B, 0xE7, 0x1F, 0xD3, 0x1D, 0xC2, 0xC1, 0xC1,
    0xC1, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, 0x51, 0xDD, 0x68, 0xD1, 0x69, 0xDD, 0x46, 0x9B, 0x4E,
    0xFB, 0x2E, 0x74, 0x6C, 0xA3, 0xF3, 0x20, 0x74, 0x80, 0x3D, 0xBE, 0xB7, 0x6D, 0x97, 0x9F, 0xDB,
    0xC2, 0x98, 0x1B, 0x3D, 0x28, 0x39, 0xD8, 0x55, 0x72, 0x43, 0xEC, 0x65, 0x62, 0xA4, 0x3B, 0x29,
    0x22, 0x53, 0x33, 0x22, 0xBA, 0x29, 0x2F, 0xE2, 0xA7, 0xE7, 0x45, 0xE2, 0xDA, 0x82, 0x50, 0x1E,
    0xD0, 0x79, 0x56, 0x13, 0xCA, 0xAB, 0x9A, 0x5F, 0xF9, 0xA4, 0x24, 0xD4, 0xAF, 0x4A, 0x7E, 0xF5,
    0xFB, 0x92, 0xBF, 0xF7, 0xB7, 0x92, 0xE8, 0xFD, 0x1B, 0xE0, 0x9F, 0xF1, 0xDF, 0xF0, 0xFF, 0xF0,
    0x35, 0xF8, 0x5A, 0x7C, 0x0F, 0xBE, 0x17, 0x3F, 0x03, 0x3F, 0x0B, 0x3F, 0x13, 0x3F, 0x1B, 0xBF,
    0x63, 0x97, 0x89, 0xB2, 0xEB, 0xE4, 0x68, 0xE1, 0xE4, 0xE0, 0x30, 0x5B, 0xF8, 0x76, 0xC8, 0xB7,
    0x4E, 0x84, 0x47, 0x25, 0xE5, 0xD8, 0x9A, 0x8C, 0x1C, 0x3F, 0xA6, 0x20, 0x12, 0xD7, 0x15, 0x85,
    0xF2, 0x9C, 0x2E, 0xEF, 0xE7, 0x3A, 0xFF, 0xD5, 0x89, 0xCF, 0xF1, 0x3B, 0xF1, 0xBB, 0xB1, 0x0D,
    0xD8, 0x16, 0x6C, 0xD3, 0xB6, 0x89, 0xD1, 0xCE, 0x09, 0xC1, 0x51, 0x53, 0xE1, 0x97, 0x7B, 0xC6,
    0x05, 0x75, 0xD1, 0x8E, 0x2A, 0x08, 0xE5, 0x36, 0x1C, 0xA1, 0x21, 0xA1, 0x15, 0xC1, 0xB6, 0x61,
    0x1B, 0xB1, 0xAD, 0xD8, 0x66, 0x4E, 0x08, 0x8E, 0x72, 0x7B, 0xF7, 0xB6, 0xAD, 0xAD, 0xCC, 0xB8,
    0xC3, 0x52, 0xBE, 0x78, 0x21, 0x2F, 0x27, 0xAE, 0xD7, 0x84, 0xFA, 0x2E, 0xE4, 0xB2, 0x23, 0xD8,
    0x76, 0xEC, 0x03, 0xF6, 0x05, 0xFB, 0x84, 0x7D, 0xDB, 0xA3, 0x65, 0xE2, 0x30, 0x06, 0xC5, 0xEF,
    0x58, 0x26, 0x8D, 0x3A, 0x32, 0x2B, 0xC7, 0x8F, 0x2D, 0xFA, 0x95, 0x17, 0x21, 0x8F, 0x13, 0xC1,
    0xBE, 0x61, 0x1F, 0xB1, 0xAF, 0xD8, 0xE7, 0xE6, 0x9E, 0x08, 0x94, 0xBE, 0x7D, 0xE6, 0xB0, 0x23,
    0x86, 0xA5, 0x45, 0x4C, 0x2B, 0x08, 0xE5, 0x29, 0x2C, 0x46, 0x21, 0x49, 0x33, 0x80, 0x7D, 0xC5,
    0x3E, 0x63, 0xDF, 0x91, 0x01, 0xB2, 0x68, 0x8E, 0xC9, 0x40, 0xF1, 0xDB, 0x82, 0xC1, 0x60, 0x57,
    0x52, 0x8E, 0x2F, 0xCC, 0x89, 0xC4, 0xCD, 0xBA, 0x08, 0x3F, 0x42, 0x88, 0x66, 0x06, 0x19, 0x20,
    0x0B, 0x64, 0x82, 0x6C, 0x90, 0x91, 0x03, 0x27, 0x02, 0x8F, 0xF6, 0x93, 0x02, 0xE3, 0x5C, 0x38,
    0x63, 0xA2, 0x09, 0xF5, 0x23, 0x4A, 0xBF, 0x6F, 0x90, 0x0D, 0x32, 0x42, 0x56, 0x0E, 0xF9, 0x54,
    0xA0, 0xF8, 0x73, 0xBC, 0x47, 0x4A, 0x19, 0x91, 0x38, 0x41, 0x2F, 0xEE, 0xD7, 0x94, 0xBC, 0x6F,
    0x20, 0x2B, 0x64, 0x86, 0xEC, 0x6C, 0x3A, 0x11, 0x28, 0xFE, 0x22, 0x57, 0x30, 0x90, 0x13, 0x89,
    0x33, 0xF5, 0x62, 0xFE, 0x40, 0xA9, 0x2B, 0x03, 0xD9, 0x21, 0x43, 0x64, 0x69, 0xFD, 0x89, 0xC0,
    0x61, 0x88, 0xBF, 0x44, 0x0A, 0xF5, 0xE8, 0x45, 0xBB, 0x58, 0x2F, 0xE0, 0x2F, 0x94, 0xB8, 0x66,
    0xFC, 0x82, 0x4C, 0x71, 0xF6, 0x68, 0x97, 0x89, 0xC0, 0x61, 0xA5, 0xC5, 0x2D, 0xCE, 0x66, 0x64,
    0x45, 0xFC, 0xF4, 0x92, 0x5F, 0xFD, 0x9D, 0xC2, 0x9A, 0x03, 0xB2, 0x45, 0xC6, 0xC8, 0x9A, 0x8B,
    0x65, 0xEB, 0xB4, 0x3B, 0x5D, 0x1B, 0x7D, 0xB1, 0xB5, 0x9A, 0x50, 0xDE, 0xA7, 0xA4, 0xF5, 0x01,
    0x59, 0x23, 0x73, 0x64, 0xCF, 0xB6, 0xA8, 0x71, 0xED, 0x4E, 0xC7, 0x72, 0x77, 0x78, 0x54, 0x5E,
    0x56, 0x1E, 0xA1, 0x94, 0x8D, 0x01, 0xD9, 0xA3, 0x06, 0xA8, 0x05, 0xDB, 0xA2, 0x3A, 0xB6, 0x3B,
    0xA3, 0xDC, 0xA3, 0x06, 0x67, 0x45, 0xEC, 0x78, 0xF6, 0xF9, 0x96, 0xE0, 0x17, 0xD4, 0x02, 0x35,
    0x61, 0x5B, 0x64, 0xFE, 0x51, 0xBF, 0x73, 0x9D, 0x1C, 0x5E, 0xA8, 0x09, 0xF5, 0x4D, 0x8A, 0x67,
    0x2D, 0x50, 0x13, 0xD4, 0x06, 0x35, 0x32, 0xE1, 0xD3, 0x80, 0xBD, 0xBE, 0x10, 0x62, 0x40, 0xDA,
    0x17, 0x3F, 0xDD, 0xBA, 0xB7, 0x2C, 0x10, 0xD4, 0x06, 0x35, 0x42, 0xAD, 0x6A, 0xB7, 0x36, 0xA0,
    0xFC, 0xED, 0xF3, 0xF5, 0xD3, 0x6F, 0x79, 0xA1, 0x3C, 0x43, 0xC1, 0xEC, 0x01, 0x6A, 0x85, 0x9A,
    0xA1, 0x76, 0x9C, 0x04, 0xD5, 0xB6, 0x3C, 0x52, 0x78, 0xB9, 0x26, 0xD4, 0x2F, 0x29, 0x96, 0xBD,
    0x40, 0xCD, 0x50, 0x3B, 0xB6, 0x44, 0x15, 0x2E, 0x74, 0xF1, 0x31, 0x9A, 0x11, 0xF1, 0x33, 0x36,
    0x09, 0xF5, 0x1F, 0x0A, 0x65, 0x4F, 0x50, 0x3B, 0xD4, 0x10, 0xB5, 0x2C, 0x63, 0x81, 0xCC, 0x96,
    0x67, 0x89, 0x34, 0xB2, 0xA7, 0x80, 0xC7, 0x0C, 0x29, 0x91, 0x23, 0x40, 0x2D, 0x51, 0xD3, 0x83,
    0xB6, 0x44, 0x94, 0x1F, 0x0F, 0xA6, 0x8C, 0x99, 0xA8, 0x09, 0xF5, 0x33, 0x8A, 0xE3, 0x2C, 0x50,
    0x53, 0xD4, 0x16, 0x35, 0xDE, 0xF7, 0x24, 0xA0, 0xFC, 0x9D, 0x6B, 0xBC, 0x91, 0x05, 0x78, 0x3B,
    0x02, 0x85, 0x71, 0x26, 0xA8, 0x2D, 0x6A, 0x8C, 0x5A, 0x73, 0x12, 0xEC, 0xB5, 0xD8, 0x0D, 0xA5,
    0xF0, 0x7A, 0x10, 0x8A, 0xE2, 0x6C, 0x50, 0x63, 0xD4, 0x7A, 0xE7, 0xE2, 0x98, 0xF2, 0x77, 0x25,
    0xA5, 0xC8, 0xE6, 0x4D, 0xA2, 0xF7, 0xAF, 0xE6, 0x90, 0x80, 0xA0, 0xD6, 0xA8, 0x39, 0x6A, 0x0F,
    0x07, 0x9A, 0x59, 0xFE, 0x7E, 0x69, 0x39, 0x76, 0x62, 0x73, 0x8A, 0x40, 0x50, 0x7B, 0x38, 0xD0,
    0x8C, 0x93, 0xA0, 0x35, 0x10, 0x08, 0xF4, 0xCF, 0xCA, 0xF1, 0xCB, 0x9B, 0x5B, 0x02, 0x02, 0x07,
    0xE0, 0x42, 0x33, 0x4D, 0x02, 0xEC, 0x28, 0xE5, 0x27, 0xBB, 0x4D, 0x02, 0x38, 0x01, 0x37, 0x9A,
    0xA4, 0xED, 0x89, 0x9E, 0xB4, 0x6B, 0x00, 0x84, 0xC0, 0x09, 0x87, 0xB7, 0x43, 0xDB, 0x1E, 0x5E,
    0x91, 0xC2, 0x5B, 0xF6, 0x15, 0x00, 0x21, 0x70, 0xC3, 0xA9, 0x0B, 0x63, 0xE3, 0x54, 0xE7, 0x5A,
    0x6F, 0x28, 0xCD, 0xB3, 0x3D, 0x64, 0x7F, 0xC0, 0x0D, 0x38, 0xE2, 0xB4, 0x53, 0xA4, 0x2D, 0xD8,
    0xA1, 0x95, 0x9E, 0xD0, 0x22, 0x9E, 0xE7, 0x27, 0x07, 0x03, 0x8E, 0xC0, 0x15, 0xA7, 0x5C, 0x2C,
    0xC3, 0x0E, 0x74, 0x2C, 0xF5, 0x8E, 0x9E, 0xC4, 0x2B, 0xBC, 0xA4, 0xAF, 0xC0, 0x15, 0x38, 0x63,
    0xF7, 0xDB, 0x26, 0xB0, 0xE1, 0xED, 0x73, 0xDC, 0x3D, 0x23, 0xB5, 0x32, 0xDF, 0x9D, 0x4F, 0x08,
    0x9C, 0x81, 0x3B, 0x76, 0xBE, 0x81, 0xAE, 0xCD, 0xED, 0x76, 0x0F, 0xCE, 0x0B, 0xE5, 0xF9, 0x4A,
    0x02, 0x20, 0x04, 0xEE, 0xC0, 0x21, 0xB8, 0x64, 0xCB, 0x73, 0xFD, 0x69, 0x39, 0x7A, 0x6E, 0x35,
    0x01, 0x10, 0x92, 0x92, 0xA3, 0xE7, 0xD8, 0xED, 0x1A, 0x81, 0xB1, 0xE8, 0x5D, 0xE5, 0x1D, 0xBF,
    0x9A, 0x0F, 0xB3, 0x54, 0x0B, 0x81, 0x43, 0x70, 0xC9, 0x2E, 0x8B, 0x62, 0xA3, 0xEF, 0x9F, 0xBA,
    0xB5, 0xEF, 0xFF, 0xAA, 0x16, 0x01, 0x10, 0x02, 0x97, 0xA6, 0xDA, 0x64, 0x3D, 0x60, 0xF4, 0xFD,
    0x39, 0x3E, 0xCD, 0x45, 0x6A, 0x0C, 0x9C, 0xB2, 0xFA, 0x7A, 0xC0, 0xE8, 0xFB, 0x93, 0xBE, 0xD8,
    0x59, 0x66, 0x04, 0x40, 0x08, 0xDC, 0xB2, 0xEA, 0x7A, 0xC0, 0xE8, 0xFB, 0x57, 0x48, 0xE3, 0x96,
    0xB1, 0xEF, 0x27, 0x66, 0xBE, 0x77, 0x08, 0x8E, 0x59, 0x71, 0x3D, 0xD0, 0xE6, 0x1F, 0xEC, 0xEF,
    0x2E, 0x0A, 0xE5, 0x6D, 0x33, 0x03, 0x20, 0x04, 0x8E, 0xC1, 0x35, 0x38, 0x67, 0xAD, 0xD6, 0x47,
    0x8E, 0x9C, 0x5A, 0x8F, 0x00, 0x08, 0x81, 0x6B, 0x16, 0x69, 0x85, 0xB6, 0x5D, 0xED, 0x1D, 0x3E,
    0x2A, 0xAC, 0x09, 0xF5, 0x57, 0x16, 0x87, 0xD4, 0x03, 0xB8, 0x06, 0xE7, 0xAC, 0x70, 0x56, 0x08,
    0x33, 0x70, 0x50, 0x4E, 0x4E, 0x3C, 0x5E, 0xCF, 0x00, 0x08, 0x81, 0x73, 0x70, 0x0F, 0x0E, 0x36,
    0xF2, 0xE8, 0xDF, 0xB5, 0xCA, 0x1B, 0x4A, 0x37, 0x22, 0x00, 0x42, 0xE0, 0x1E, 0x1C, 0x6C, 0xD4,
    0xA7, 0x40, 0xDB, 0xF8, 0x81, 0x5E, 0xA9, 0x28, 0x94, 0x0F, 0x59, 0x8C, 0x46, 0x40, 0xE0, 0x1E,
    0x1C, 0x84, 0x8B, 0x0D, 0x5A, 0xF8, 0x36, 0xF6, 0x5E, 0x1F, 0x42, 0xE0, 0x60, 0xBD, 0x17, 0xC4,
    0xC6, 0xC2, 0x77, 0x8A, 0x2B, 0x38, 0xAE, 0xD1, 0x7F, 0x20, 0x1D, 0x21, 0x70, 0x10, 0x2E, 0xD6,
    0x73, 0x41, 0x8C, 0x99, 0x36, 0x20, 0x25, 0xC7, 0xAE, 0xB2, 0x42, 0x00, 0x84, 0xC0, 0x45, 0xDD,
    0xC9, 0x81, 0x70, 0xB3, 0x2E, 0x47, 0xFF, 0xDE, 0xC1, 0xFE, 0x91, 0x56, 0x39, 0xED, 0x49, 0x08,
    0x5C, 0x84, 0x93, 0xF5, 0xF8, 0x14, 0x30, 0x8E, 0xFE, 0x1B, 0x7D, 0xB1, 0x0B, 0xAD, 0x14, 0x00,
    0x21, 0x70, 0x12, 0x6E, 0xC2, 0x51, 0x53, 0x8F, 0xFE, 0xD1, 0x61, 0xBE, 0x23, 0x34, 0xA1, 0xFE,
    0xC8, 0xD0, 0xAD, 0x04, 0x81, 0x93, 0x70, 0xD3, 0xCC, 0x4F, 0x01, 0xE3, 0xE8, 0xBF, 0xC1, 0x17,
    0x3D, 0xC3, 0x8A, 0x01, 0x10, 0x02, 0x37, 0xCD, 0xFA, 0x14, 0x30, 0x8E, 0xFE, 0x41, 0x97, 0x3F,
    0xA0, 0x09, 0xF5, 0x5B, 0x86, 0x6D, 0x45, 0x08, 0xDC, 0x84, 0xA3, 0x66, 0x7C, 0x0A, 0x18, 0xE7,
    0xFD, 0x37, 0xF8, 0x22, 0xC7, 0x5B, 0x39, 0x00, 0x42, 0xE0, 0xA8, 0x19, 0xD7, 0x05, 0xDA, 0x86,
    0x0C, 0x19, 0xE2, 0x2A, 0x0A, 0xE5, 0x13, 0x86, 0x6C, 0x65, 0x08, 0x1C, 0x85, 0xAB, 0x70, 0xB6,
    0xA6, 0xF7, 0xFC, 0x2C, 0xF1, 0x8C, 0x5D, 0xC7, 0x80, 0x89, 0x1D, 0x58, 0xEA, 0x19, 0xB7, 0xB6,
    0x96, 0xF7, 0x08, 0xB5, 0xEA, 0x0C, 0x4E, 0xC9, 0xB1, 0xBB, 0x19, 0x2E, 0xB1, 0x03, 0x70, 0x15,
    0xCE, 0xD6, 0xAA, 0x0D, 0x6A, 0xEF, 0xE9, 0x3F, 0xFC, 0x30, 0x4D, 0xA8, 0x3F, 0x33, 0x5C, 0x62,
    0x07, 0xE0, 0x2A, 0x9C, 0x85, 0xBB, 0xB5, 0x68, 0x7F, 0xFA, 0xAD, 0x96, 0xC6, 0x6F, 0x66, 0xB0,
    0xC4, 0x4E, 0xC0, 0x59, 0xB8, 0x5B, 0x6D, 0x1B, 0xD4, 0xA6, 0x33, 0x34, 0x2B, 0xC7, 0x9F, 0x61,
    0xA8, 0xC4, 0x4E, 0xC0, 0x59, 0xB8, 0x0B, 0x87, 0xAB, 0x7A, 0xB3, 0xF3, 0xA4, 0xE1, 0x81, 0x10,
    0x9E, 0xC6, 0x67, 0xA8, 0xC4, 0x4E, 0xC0, 0x59, 0xB8, 0x5B, 0xCD, 0x1B, 0xA6, 0xF1, 0x4D, 0x03,
    0xD6, 0x4B, 0xE1, 0x93, 0x19, 0x26, 0xB1, 0x23, 0x70, 0x17, 0x0E, 0xC3, 0xE5, 0x4A, 0xDB, 0x1F,
    0x57, 0x5E, 0x24, 0x5E, 0x63, 0x98, 0xC4, 0x8E, 0xC0, 0x5D, 0x38, 0x0C, 0x97, 0x2B, 0x6A, 0x7F,
    0x12, 0x68, 0x7F, 0x18, 0x24, 0xB1, 0x31, 0x89, 0x0A, 0xDB, 0x20, 0x7C, 0x71, 0xFF, 0x15, 0x9E,
    0xF1, 0x5B, 0x18, 0x22, 0xB1, 0x33, 0x70, 0x18, 0x2E, 0x97, 0x3B, 0x01, 0x5A, 0x75, 0x86, 0x24,
    0x7D, 0xD1, 0x3B, 0x19, 0x22, 0xB1, 0x33, 0x70, 0x18, 0x2E, 0x97, 0x7B, 0x51, 0xAC, 0x5D, 0xC7,
    0xC3, 0x37, 0x3E, 0x10, 0xBB, 0x03, 0x87, 0xE1, 0x32, 0x9C, 0x2E, 0xEB, 0x45, 0xB7, 0x93, 0x87,
    0xF4, 0xA8, 0x0C, 0x90, 0x38, 0x01, 0xB8, 0x5C, 0xCE, 0x0B, 0x75, 0x8D, 0x07, 0x5F, 0x56, 0x79,
    0xC7, 0x1F, 0xCD, 0xF0, 0x88, 0x13, 0x80, 0xCB, 0xE5, 0x3C, 0x28, 0x83, 0x2F, 0x1A, 0x9A, 0xF6,
    0xC5, 0xEE, 0x67, 0x78, 0xC4, 0x09, 0xC0, 0x65, 0x38, 0x0D, 0xB7, 0xFB, 0xDA, 0xFF, 0x7B, 0x8B,
    0x42, 0xF9, 0x9C, 0xE1, 0x11, 0x27, 0x00, 0x97, 0xE1, 0x34, 0xDC, 0xEE, 0x53, 0xFF, 0x3F, 0xC5,
    0x75, 0xC4, 0x44, 0x06, 0x47, 0x9C, 0x04, 0x9C, 0xEE, 0xCB, 0x3A, 0xC0, 0x38, 0xFF, 0xBF, 0xC4,
    0x33, 0xA6, 0xC4, 0xD0, 0x88, 0x93, 0x80, 0xD3, 0x7D, 0xB9, 0x1E, 0x60, 0x3C, 0xFC, 0xB2, 0x46,
    0x0A, 0x9D, 0xC6, 0xD0, 0x88, 0x93, 0xF8, 0x9F, 0xBD, 0x73, 0x00, 0x92, 0xED, 0x88, 0xC2, 0x70,
    0x6C, 0xAF, 0xBB, 0xEF, 0xB3, 0x39, 0x6B, 0x5B, 0xA3, 0x67, 0xDB, 0xB6, 0x6D, 0xDB, 0x36, 0x62,
    0xDB, 0xB6, 0x93, 0xB2, 0x92, 0x32, 0xC2, 0x52, 0xCA, 0x3A, 0xE9, 0xBF, 0x72, 0xEB, 0x61, 0x32,
    0x7D, 0xD7, 0xE8, 0x9E, 0x53, 0x55, 0x7F, 0xE1, 0x71, 0xEF, 0xEE, 0xF7, 0xDF, 0x39, 0xEC, 0x06,
    0xD3, 0xB5, 0x59, 0x92, 0xB9, 0x5D, 0xE9, 0x51, 0x6E, 0x80, 0xB1, 0x2C, 0x6D, 0x88, 0x3D, 0x0A,
    0xC6, 0x6B, 0x6C, 0x80, 0x4D, 0x48, 0x49, 0xFF, 0xDE, 0xAC, 0x07, 0x64, 0x95, 0x6D, 0xDF, 0x4D,
    0xA1, 0x99, 0x4B, 0x68, 0x72, 0x87, 0xFC, 0xC6, 0x5D, 0x2F, 0xCC, 0x0E, 0x51, 0xC9, 0x9E, 0xFD,
    0x34, 0x34, 0x3C, 0xC6, 0xE8, 0xEF, 0x0F, 0x98, 0xAE, 0x4D, 0x43, 0x0C, 0xBF, 0x99, 0xA2, 0xB2,
    0xE6, 0xBF, 0x18, 0x2A, 0xB3, 0x94, 0x7F, 0xF6, 0x2C, 0xF5, 0xF8, 0xE9, 0x33, 0xEA, 0xF3, 0xC9,
    0xDB, 0x54, 0xB5, 0x7C, 0x3D, 0x4D, 0xE8, 0x51, 0xD6, 0xB0, 0x51, 0xE2, 0x8A, 0xE1, 0x54, 0x70,
    0xFC, 0x04, 0xF5, 0xF8, 0xF1, 0x53, 0xC2, 0xBF, 0x3B, 0x64, 0xE0, 0x04, 0xD3, 0x2B, 0x41, 0x7F,
    0x81, 0xED, 0x9A, 0x0C, 0x70, 0x57, 0x97, 0x07, 0xE2, 0x7A, 0x30, 0x50, 0xC6, 0x1A, 0xE0, 0x9A,
    0x7A, 0x7E, 0xFB, 0x11, 0xDE, 0xDC, 0x34, 0xB2, 0x68, 0x50, 0xED, 0x77, 0x69, 0xDB, 0xE4, 0xD2,
    0xA0, 0x11, 0xD3, 0x28, 0xFB, 0x89, 0xAB, 0x37, 0xFD, 0x5B, 0x36, 0x18, 0x00, 0xEA, 0xFA, 0x60,
    0x5C, 0x77, 0x30, 0xEE, 0xB9, 0xFF, 0x5B, 0x19, 0xD7, 0x79, 0x20, 0x03, 0x65, 0xB6, 0x01, 0x22,
    0x05, 0xA0, 0xFB, 0x4F, 0x98, 0xAB, 0x0D, 0x8F, 0xC6, 0xF9, 0xAA, 0xA8, 0x7A, 0xF9, 0x3A, 0xF2,
    0xBD, 0xF7, 0xDA, 0xB5, 0xBF, 0x63, 0xA3, 0x01, 0xC0, 0xB6, 0xD7, 0x9E, 0x30, 0xB2, 0xE3, 0xFB,
    0x07, 0x26, 0xF6, 0xB4, 0x6B, 0x04, 0x82, 0x0D, 0x70, 0x4D, 0xBD, 0xBF, 0x7C, 0x9F, 0x8A, 0x0F,
    0x1E, 0xA2, 0x81, 0x63, 0x66, 0xD2, 0xF0, 0xAA, 0x91, 0xC8, 0x19, 0x28, 0xF7, 0xD2, 0x85, 0x6B,
    0x61, 0x0E, 0x64, 0xB3, 0x01, 0xC0, 0xB6, 0xFE, 0x1E, 0x01, 0x77, 0x04, 0x62, 0x44, 0x72, 0xDF,
    0x93, 0x76, 0x42, 0xC2, 0x06, 0xF0, 0x96, 0xFD, 0x06, 0x00, 0xDB, 0xFA, 0x91, 0x08, 0x77, 0x05,
    0x72, 0x74, 0xB2, 0xEF, 0x79, 0x06, 0xCA, 0x46, 0x03, 0xB0, 0x01, 0xC0, 0xB6, 0xD7, 0x8A, 0x24,
    0xB2, 0xE3, 0xF8, 0x31, 0xC9, 0x69, 0x1F, 0x30, 0x50, 0x36, 0x1A, 0x80, 0x0D, 0x00, 0xB6, 0xC1,
    0x38, 0x58, 0xD7, 0x19, 0x20, 0x49, 0x9D, 0xA7, 0xF2, 0x03, 0x03, 0x65, 0xA3, 0x01, 0xD8, 0x00,
    0x60, 0x1B, 0x8C, 0x7B, 0x19, 0x20, 0x65, 0xA2, 0xC8, 0xF8, 0x85, 0x81, 0xB2, 0xD1, 0x00, 0x6C,
    0x00, 0xB0, 0xED, 0xD5, 0x0B, 0xB8, 0x53, 0x49, 0x4C, 0x12, 0x19, 0xBF, 0x33, 0x50, 0x36, 0x1A,
    0x80, 0x0D, 0x00, 0xB6, 0xC1, 0x38, 0x58, 0xD7, 0x19, 0xC0, 0xDA, 0x1B, 0x60, 0xD8, 0x00, 0x6C,
    0x00, 0xB0, 0x0D, 0xC6, 0x75, 0x06, 0xB8, 0x4B, 0xA9, 0xCD, 0x14, 0x91, 0xF9, 0x0F, 0x03, 0x65,
    0x9E, 0x02, 0x8B, 0x56, 0x51, 0x9F, 0x4F, 0xDF, 0x61, 0x03, 0x78, 0x08, 0x6C, 0x83, 0x71, 0xB0,
    0xAE, 0x35, 0x40, 0xC3, 0x6F, 0x7F, 0x67, 0xCD, 0x16, 0x39, 0xB4, 0xCE, 0x29, 0xA4, 0xBD, 0xB2,
    0x9C, 0x8E, 0xCB, 0x4A, 0x3A, 0xEF, 0x54, 0xD3, 0x15, 0x27, 0x40, 0x8F, 0x2B, 0x3D, 0xE1, 0x04,
    0xE9, 0x49, 0x8D, 0xF0, 0x7B, 0x8F, 0xCB, 0x00, 0x5D, 0x51, 0xBA, 0xA0, 0xFE, 0xCE, 0x09, 0xA7,
    0x92, 0xF6, 0x8B, 0x72, 0xDA, 0x28, 0x8A, 0x69, 0xAE, 0xC8, 0xA9, 0xD5, 0x28, 0xC3, 0xB0, 0xF0,
    0x58, 0x2A, 0xDD, 0xB5, 0x87, 0x7A, 0x7F, 0xF1, 0x1E, 0x1B, 0x20, 0x42, 0x60, 0xBB, 0x26, 0x03,
    0xB4, 0xE5, 0x83, 0x70, 0xEB, 0xAE, 0xD5, 0x0A, 0xF6, 0x43, 0xB2, 0x5C, 0x41, 0xEB, 0x57, 0x10,
    0x07, 0x00, 0x73, 0x93, 0x08, 0x06, 0xB9, 0x20, 0xFD, 0x74, 0xD8, 0xA9, 0xA0, 0x75, 0xA2, 0xC8,
    0xF3, 0x6B, 0xC2, 0xD8, 0x43, 0xFF, 0x89, 0x73, 0x29, 0xEB, 0xA9, 0xC7, 0xD9, 0x00, 0xAE, 0xC0,
    0x36, 0x18, 0x6F, 0x04, 0x03, 0xF0, 0x1B, 0x7E, 0x97, 0x2C, 0xA5, 0x73, 0xD2, 0xDF, 0x64, 0xB0,
    0xD7, 0xD6, 0x10, 0x30, 0xDD, 0x3E, 0xA7, 0xD4, 0xF3, 0x13, 0x62, 0x64, 0xF9, 0x30, 0x2A, 0x3C,
    0x79, 0xA2, 0x36, 0xA0, 0xB3, 0x01, 0xA2, 0x87, 0x40, 0xAC, 0xE9, 0x0A, 0xB0, 0x1D, 0xA2, 0x94,
    0x2E, 0xCA, 0x6A, 0xC0, 0xD7, 0x2A, 0x75, 0x49, 0x99, 0x61, 0xB7, 0x28, 0xA3, 0x99, 0x1A, 0x33,
    0x8C, 0xC9, 0xE9, 0x47, 0x45, 0x87, 0x8E, 0xC4, 0x7A, 0x08, 0xE4, 0x6D, 0x80, 0x9B, 0x93, 0x60,
    0xD6, 0x12, 0x91, 0x47, 0x27, 0x65, 0x25, 0x00, 0x33, 0x4A, 0xA7, 0x65, 0x15, 0xAD, 0x70, 0xA2,
    0x4F, 0x7F, 0x8E, 0x2A, 0x1E, 0x4C, 0x79, 0x17, 0xCF, 0xC7, 0x68, 0x12, 0xEC, 0x6D, 0x80, 0x36,
    0x5C, 0x06, 0xFD, 0x4F, 0x6B, 0x45, 0x21, 0xC2, 0x0B, 0xC0, 0x64, 0xB4, 0x2E, 0x39, 0xD5, 0xB4,
    0x41, 0x16, 0x47, 0xBF, 0x46, 0x68, 0xF0, 0x44, 0x4A, 0x7B, 0xE7, 0xD5, 0x58, 0x2B, 0x83, 0x7A,
    0x57, 0x81, 0x62, 0xBD, 0x11, 0x86, 0x84, 0xF6, 0x92, 0x0B, 0xBE, 0x4D, 0xBA, 0xAC, 0x9E, 0x69,
    0xBD, 0x28, 0x8A, 0x9A, 0x2C, 0x57, 0xAE, 0xDB, 0x4C, 0x3D, 0xBF, 0xFF, 0x38, 0x36, 0x1A, 0x61,
    0xAE, 0x01, 0xB4, 0x8D, 0xB0, 0x89, 0x29, 0x19, 0xBF, 0xC6, 0x64, 0xA8, 0xE3, 0xE4, 0xA3, 0x5C,
    0x09, 0x58, 0xAC, 0xD6, 0x45, 0xE9, 0xA7, 0x65, 0x22, 0x3F, 0x6A, 0x58, 0x94, 0xFE, 0xDA, 0x0B,
    0x56, 0x1B, 0x00, 0x6C, 0x83, 0x71, 0xAF, 0x4E, 0xB0, 0x18, 0x97, 0x92, 0xFE, 0x63, 0x4C, 0x25,
    0xB7, 0x4A, 0xC7, 0xF4, 0x31, 0xBE, 0xB5, 0x42, 0x8F, 0x01, 0xC9, 0x72, 0xE4, 0xA7, 0x41, 0xF9,
    0xE6, 0xED, 0xD6, 0x1A, 0x00, 0x6C, 0x7B, 0x8D, 0x42, 0xDC, 0xA1, 0x94, 0xAC, 0x8E, 0x8F, 0xF8,
    0x28, 0x56, 0xE0, 0x47, 0x6C, 0xFC, 0xB8, 0xBE, 0x6E, 0x6F, 0xBD, 0x1E, 0x97, 0x41, 0xDA, 0x1C,
    0x25, 0x3F, 0x18, 0x3C, 0x64, 0x32, 0xF5, 0xFE, 0xFC, 0x3D, 0xEB, 0x0C, 0x00, 0xB6, 0xC1, 0xB8,
    0xE7, 0x38, 0xF4, 0xC8, 0x24, 0xDF, 0x4B, 0xB1, 0xF0, 0xD6, 0x3F, 0xE9, 0x54, 0xB9, 0x20, 0xB0,
    0xCE, 0x38, 0x55, 0xFF, 0xFB, 0x34, 0x18, 0x9B, 0x19, 0xA4, 0x8C, 0x97, 0x9E, 0xB5, 0x6B, 0x23,
    0x2C, 0xC9, 0xF7, 0x62, 0x4D, 0xE3, 0xD0, 0xF1, 0x43, 0x93, 0x7A, 0x9F, 0xB3, 0x19, 0xFE, 0x15,
    0xB2, 0x20, 0xCA, 0x5B, 0x9F, 0xF5, 0xB8, 0xD2, 0x4A, 0x51, 0x78, 0x73, 0xD2, 0xD8, 0xA1, 0x80,
    0xF2, 0x4F, 0x9D, 0xB6, 0xC6, 0x00, 0x60, 0x5B, 0xBF, 0x10, 0xE3, 0xAE, 0x44, 0x86, 0x13, 0xBA,
    0xAD, 0xB5, 0x15, 0x7E, 0x34, 0xB2, 0xBC, 0x41, 0x60, 0xED, 0x96, 0x11, 0xE7, 0x09, 0xC9, 0x6C,
    0xE4, 0x05, 0x56, 0x18, 0x00, 0x6C, 0xEB, 0x57, 0x22, 0xDD, 0xA5, 0xF8, 0xE2, 0xC7, 0xDA, 0x8F,
    0xB4, 0x11, 0xFE, 0x93, 0xB2, 0xB6, 0x21, 0x0F, 0xEB, 0x94, 0x53, 0x45, 0xD3, 0xA3, 0x94, 0x4B,
    0x4D, 0x67, 0x00, 0x6C, 0xEB, 0x97, 0xE2, 0xDD, 0x63, 0x51, 0xDA, 0xDD, 0xF7, 0x70, 0xBA, 0x45,
    0xE0, 0x23, 0xB6, 0xB5, 0xA2, 0xA1, 0xD5, 0x12, 0x0D, 0x34, 0x37, 0x2F, 0xB0, 0x46, 0x60, 0x5B,
    0x7F, 0x2C, 0x8A, 0x7B, 0x30, 0x96, 0x52, 0xC7, 0xC9, 0x22, 0xF3, 0x6F, 0x1B, 0x1E, 0x78, 0x8E,
    0xC8, 0xA1, 0x2B, 0xF5, 0x8E, 0xF7, 0x59, 0x57, 0xA5, 0x9F, 0xE6, 0x89, 0x5C, 0x2B, 0xE0, 0x07,
    0xD3, 0x60, 0xDB, 0xEB, 0x60, 0xAC, 0x6B, 0xDD, 0xE0, 0x71, 0x29, 0x69, 0x3F, 0x9B, 0xFE, 0xC0,
    0xF3, 0x65, 0x2E, 0x5D, 0x6D, 0x30, 0xFC, 0x2C, 0xEC, 0x27, 0x2C, 0x10, 0x79, 0xC6, 0x1B, 0x00,
    0x4C, 0x6B, 0xBB, 0xC0, 0x91, 0x8B, 0xF1, 0x23, 0x92, 0xFA, 0xBE, 0x62, 0x3A, 0xFC, 0x8F, 0x37,
    0x2A, 0x08, 0x5C, 0x21, 0x9A, 0x65, 0x78, 0x38, 0x04, 0xA6, 0x6B, 0x73, 0x38, 0x2E, 0x7E, 0x33,
    0x61, 0x40, 0x42, 0xAF, 0x83, 0x06, 0x87, 0x3D, 0xFC, 0xE6, 0x6F, 0x02, 0x9D, 0x91, 0xE8, 0x15,
    0x64, 0x1B, 0x6B, 0x00, 0x30, 0xAD, 0x3F, 0x1E, 0x3D, 0xE2, 0x82, 0x8C, 0xCA, 0xB8, 0xAE, 0xF3,
    0x8C, 0x4C, 0x78, 0x39, 0xE6, 0x6F, 0x52, 0x9D, 0x94, 0x95, 0xC6, 0x26, 0xC6, 0x60, 0x5A, 0x7F,
    0x41, 0x46, 0xC4, 0x15, 0x49, 0x69, 0x0F, 0x8B, 0x4A, 0x13, 0x1F, 0xF2, 0x22, 0x57, 0x7B, 0x9A,
    0x5C, 0xA8, 0xA8, 0x99, 0xC8, 0x06, 0x98, 0xD6, 0x5F, 0x91, 0x14, 0x71, 0x49, 0x9E, 0x52, 0x67,
    0x35, 0x3A, 0xFA, 0x07, 0xD7, 0xF9, 0x59, 0x9A, 0x41, 0x3A, 0xA3, 0xE0, 0x07, 0xCB, 0x60, 0xBA,
    0x36, 0x97, 0xE4, 0xDD, 0xEA, 0x66, 0xC9, 0xCE, 0xC8, 0x64, 0xDF, 0xDB, 0xA6, 0x3C, 0xE0, 0x66,
    0x51, 0xD2, 0x8C, 0x00, 0xB0, 0x2E, 0x4B, 0x3F, 0xAD, 0x77, 0x8A, 0x8D, 0x31, 0x00, 0x58, 0x06,
    0xD3, 0xDE, 0xD7, 0xA4, 0x46, 0x5C, 0x94, 0x1D, 0x8C, 0xEF, 0xBE, 0xD9, 0xAC, 0x01, 0xB7, 0x6C,
    0x5A, 0x28, 0x72, 0x69, 0x8D, 0x2C, 0xA0, 0x6D, 0xB2, 0x84, 0x0E, 0xC9, 0x0A, 0x3A, 0x27, 0xAB,
    0xE9, 0x09, 0x06, 0xB6, 0xDE, 0xBA, 0xDA, 0x3E, 0x84, 0xB7, 0x3D, 0xED, 0x91, 0x65, 0xB4, 0x51,
    0x16, 0xD3, 0x52, 0x91, 0x6F, 0x64, 0x35, 0x08, 0x2C, 0xEB, 0x2E, 0xCA, 0xD6, 0x8E, 0x44, 0xF4,
    0xB8, 0x3F, 0xC9, 0xBC, 0x3C, 0x40, 0x63, 0x8C, 0xC5, 0x22, 0x0F, 0xDB, 0x50, 0xB4, 0xC7, 0x29,
    0xC5, 0x0F, 0x94, 0xAB, 0x44, 0x51, 0x74, 0xDE, 0xF1, 0xE3, 0xA5, 0x81, 0x97, 0x07, 0xAD, 0x94,
    0x05, 0x34, 0x47, 0xDA, 0xD3, 0x05, 0x06, 0xCB, 0x5E, 0x23, 0x10, 0xFF, 0xB2, 0x77, 0x0D, 0x40,
    0xAE, 0x6C, 0x41, 0xB4, 0xF0, 0x6D, 0x23, 0xB3, 0xCF, 0xB6, 0x9D, 0x3C, 0xDB, 0xB6, 0x6D, 0xDB,
    0xB6, 0x6D, 0xDB, 0xB6, 0xBD, 0x46, 0xB0, 0xB6, 0x6D, 0xE3, 0xFC, 0xED, 0xD9, 0xEC, 0x2B, 0xA5,
    0xDE, 0xCE, 0xCD, 0xE4, 0x4E, 0xBE, 0xBA, 0xAA, 0x0B, 0x8F, 0xC1, 0xE9, 0x73, 0xCE, 0xED, 0x9E,
    0xDB, 0x6B, 0xAA, 0x00, 0xBE, 0x32, 0x9E, 0x03, 0x42, 0x15, 0x7B, 0x4A, 0xB3, 0x7E, 0x57, 0x5C,
    0x5E, 0xB3, 0x1B, 0x2E, 0x0F, 0x5F, 0x22, 0x48, 0xEF, 0x85, 0x50, 0x4F, 0x5F, 0x78, 0xBE, 0x73,
    0xC4, 0xAB, 0xB3, 0x37, 0x70, 0x69, 0xD5, 0x4E, 0xEC, 0x1C, 0x36, 0x03, 0x0B, 0x1A, 0xF7, 0xC0,
    0x68, 0xA1, 0xBE, 0xC5, 0x66, 0x05, 0x0B, 0x05, 0xB5, 0xB8, 0xDE, 0x64, 0x8F, 0xF0, 0xDF, 0x2A,
    0x8A, 0xA3, 0x22, 0xD8, 0x5B, 0x62, 0xA5, 0xD0, 0x0C, 0xB3, 0x85, 0x26, 0x16, 0x6B, 0x6F, 0xCE,
    0xAC, 0xD9, 0x1E, 0x9B, 0xFA, 0x4E, 0xC4, 0x99, 0x85, 0x1B, 0xF1, 0xF4, 0xD8, 0x25, 0x68, 0x9F,
    0xBD, 0x45, 0xA0, 0x9B, 0x07, 0xFC, 0x9C, 0xF5, 0x70, 0xB8, 0xF3, 0x14, 0x17, 0x56, 0x6C, 0xC7,
    0xEC, 0xBA, 0x9D, 0x94, 0x5E, 0x88, 0x1B, 0x62, 0xF4, 0xFF, 0x5F, 0x49, 0x2D, 0x80, 0x0F, 0xE7,
    0x80, 0x3E, 0xBF, 0x57, 0xBF, 0xC5, 0x7D, 0xC7, 0x4E, 0x19, 0x0D, 0x9E, 0x1C, 0xBD, 0x88, 0xDC,
    0x9C, 0x1C, 0x48, 0x89, 0xAC, 0xF4, 0x0C, 0x04, 0x6A, 0x3D, 0xF0, 0xEE, 0xCA, 0x5D, 0x5C, 0x59,
    0xBB, 0x9B, 0x0A, 0x83, 0x8A, 0x47, 0xF6, 0xEB, 0x18, 0x9D, 0x9F, 0x64, 0xA1, 0x96, 0xAA, 0x34,
    0xD8, 0x6C, 0xD3, 0x02, 0x87, 0xFE, 0x25, 0xD7, 0x23, 0xC9, 0x02, 0x52, 0x83, 0x60, 0x9D, 0x4D,
    0x73, 0xCC, 0x57, 0xA9, 0x69, 0x4E, 0x22, 0x9F, 0x3C, 0x2A, 0xB5, 0xC4, 0xFA, 0x1E, 0x63, 0x71,
    0x7A, 0xE1, 0x06, 0x3C, 0x3B, 0x71, 0x19, 0x5E, 0xB6, 0xCE, 0x48, 0x89, 0x4F, 0x84, 0x94, 0xC8,
    0xC9, 0xCE, 0xC6, 0xE3, 0x23, 0xE7, 0x31, 0xA1, 0xB4, 0x5A, 0x91, 0x02, 0x20, 0x0C, 0x4B, 0xF3,
    0xFF, 0x26, 0x06, 0x62, 0x6D, 0x7F, 0xE6, 0xFB, 0x68, 0x34, 0x31, 0x46, 0xB8, 0x4F, 0x00, 0x2C,
    0x11, 0xE9, 0xC9, 0x29, 0xF0, 0xB1, 0x77, 0xC5, 0xB3, 0x93, 0x57, 0x70, 0x6A, 0xFE, 0x7A, 0xAC,
    0xE9, 0x32, 0x12, 0x13, 0xCB, 0xCA, 0x3B, 0xA8, 0x8D, 0x17, 0x1A, 0x60, 0xBE, 0xD0, 0x04, 0xEB,
    0x84, 0xE6, 0x38, 0xF0, 0x0F, 0xB9, 0x40, 0x43, 0x77, 0x1D, 0xF6, 0x08, 0xAD, 0xB0, 0xCA, 0xE8,
    0xD9, 0xC7, 0xC8, 0x64, 0x77, 0x52, 0xDD, 0x7D, 0x63, 0xE7, 0xE3, 0xF6, 0x8E, 0xA3, 0x70, 0x7D,
    0xF4, 0x0A, 0xB1, 0x21, 0x11, 0xB0, 0x44, 0xD0, 0xF7, 0x3E, 0xB3, 0x56, 0x07, 0xEE, 0x05, 0x40,
    0x18, 0x96, 0x32, 0x00, 0x33, 0x65, 0x83, 0xBE, 0x2B, 0xFB, 0xCD, 0x2F, 0xCD, 0xB8, 0x4D, 0x6C,
    0xCB, 0x35, 0x43, 0x88, 0x87, 0x0F, 0x78, 0x46, 0x6E, 0x6E, 0x2E, 0x22, 0x7C, 0x03, 0x61, 0x77,
    0xE3, 0x21, 0x2E, 0xAD, 0xDE, 0x85, 0x2D, 0xFD, 0x26, 0x61, 0x6A, 0x95, 0xD6, 0xB2, 0x06, 0x6D,
    0xE4, 0x8F, 0x37, 0x08, 0xCD, 0xD9, 0x15, 0x82, 0xF3, 0xE3, 0xCB, 0x6B, 0x84, 0x66, 0x22, 0xE0,
    0x47, 0x9B, 0xAB, 0x80, 0x42, 0x7D, 0x2C, 0x6E, 0xD6, 0x17, 0x87, 0xA7, 0x2E, 0xC3, 0xA3, 0xC3,
    0xE7, 0x88, 0xD5, 0x45, 0x52, 0xE1, 0x19, 0x64, 0x73, 0x27, 0x95, 0x6F, 0xCE, 0xB5, 0x00, 0x08,
    0xC3, 0x84, 0x65, 0x49, 0xF6, 0xC7, 0xC4, 0x3C, 0xA0, 0xCC, 0x10, 0x55, 0x6D, 0x6F, 0x1E, 0x2F,
    0xEC, 0xFE, 0xDE, 0x53, 0xB0, 0x56, 0xC4, 0x04, 0x87, 0xC1, 0xFE, 0xD6, 0x63, 0xF1, 0x6C, 0xB1,
    0xB1, 0xF7, 0x78, 0x2A, 0x46, 0xB3, 0xDE, 0x03, 0xAD, 0x23, 0x5C, 0x26, 0x68, 0xB0, 0x53, 0x68,
    0xA5, 0xE8, 0x2D, 0x33, 0x3A, 0xAF, 0xD0, 0x81, 0x75, 0x91, 0xA0, 0x36, 0xBB, 0x2B, 0x43, 0xB6,
    0x71, 0xDF, 0xB8, 0x05, 0x78, 0xB0, 0xFF, 0xB4, 0x08, 0xF6, 0x8C, 0xD4, 0x34, 0x58, 0x23, 0x1E,
    0x1C, 0x38, 0xC3, 0xEF, 0x27, 0xC2, 0xFC, 0x51, 0xDB, 0x9B, 0x30, 0x5C, 0xD8, 0xFF, 0x67, 0x2D,
    0x00, 0x71, 0x4B, 0x44, 0xD7, 0x5F, 0x2B, 0xED, 0xB6, 0xC8, 0x61, 0xA4, 0x5C, 0x53, 0x74, 0x1A,
    0x33, 0x1D, 0x9A, 0x6D, 0x3B, 0xD0, 0xF8, 0xD2, 0x79, 0x0C, 0xF4, 0x74, 0xC1, 0x9C, 0x00, 0x4F,
    0x1C, 0x8D, 0x0A, 0xC1, 0x9B, 0xA4, 0x78, 0xC4, 0x64, 0x67, 0xC1, 0x5A, 0x41, 0x4A, 0x41, 0x6C,
    0xF4, 0xF2, 0xEC, 0x75, 0x1C, 0x9B, 0xB9, 0x8A, 0x98, 0x90, 0xF9, 0xFD, 0x8D, 0x55, 0xD5, 0xC7,
    0x02, 0x41, 0x5C, 0x92, 0x4B, 0xC5, 0xC0, 0x05, 0xF4, 0x5B, 0x6D, 0x5A, 0x92, 0x25, 0x63, 0xB6,
    0x35, 0xE3, 0x4A, 0x35, 0xC1, 0xBA, 0x6E, 0xA3, 0xC5, 0x82, 0x77, 0xBA, 0xFF, 0x1C, 0x89, 0xD1,
    0xB1, 0xB0, 0x66, 0x84, 0xA7, 0xA7, 0xE1, 0x79, 0x64, 0x18, 0xF6, 0x78, 0xEA, 0x31, 0xC5, 0xF6,
    0x35, 0xEA, 0xED, 0xDD, 0x85, 0x5A, 0xDB, 0xB7, 0xA2, 0xD1, 0xE2, 0x15, 0x68, 0x3F, 0x70, 0x1C,
    0x86, 0x96, 0xB3, 0xCC, 0x8C, 0x81, 0xB0, 0x5B, 0xB8, 0x05, 0x82, 0xB5, 0x00, 0x3E, 0x5C, 0x91,
    0x2C, 0xFF, 0xF5, 0xCF, 0xAD, 0xE5, 0x2E, 0xCC, 0xED, 0x3C, 0x72, 0x0A, 0xAA, 0xBE, 0x7B, 0x5C,
    0xE4, 0xF6, 0xB1, 0xC6, 0xDA, 0xF7, 0x18, 0xE5, 0xA3, 0xC3, 0xC6, 0x50, 0x7F, 0xDC, 0x88, 0x8B,
    0x82, 0x47, 0x7A, 0x0A, 0xB2, 0xF2, 0xF2, 0x60, 0x8D, 0x48, 0x4D, 0x48, 0x22, 0xCF, 0x4B, 0xD6,
    0x49, 0x3C, 0x4F, 0x8C, 0x2D, 0xDE, 0x90, 0xC9, 0x2A, 0x2D, 0x16, 0x34, 0xD8, 0x63, 0x23, 0x7F,
    0xDD, 0x0A, 0x75, 0xA8, 0x16, 0xA9, 0x34, 0x54, 0x60, 0x4C, 0xF6, 0x72, 0xDB, 0xC0, 0xA9, 0xB8,
    0xB3, 0xEB, 0x18, 0xBC, 0xED, 0x5C, 0x91, 0x9D, 0x95, 0x0D, 0x6B, 0x44, 0x7A, 0x4E, 0x0E, 0xB4,
    0x09, 0x71, 0xB8, 0x14, 0xE8, 0x87, 0x55, 0x5A, 0x27, 0x0C, 0x7A, 0xFD, 0x14, 0xB5, 0xEE, 0x5C,
    0x41, 0x99, 0xEB, 0xE7, 0x3E, 0x9A, 0xE5, 0x2E, 0x9E, 0x42, 0xDB, 0xA1, 0x13, 0xE8, 0x3A, 0xA6,
    0xAC, 0x45, 0xB8, 0x84, 0x5D, 0x93, 0x57, 0x20, 0x19, 0x54, 0x40, 0x6C, 0x87, 0x0E, 0xFC, 0xA3,
    0xA6, 0x9D, 0xB9, 0x2F, 0xA4, 0xCD, 0xDC, 0x25, 0xB2, 0xB6, 0x12, 0x57, 0x73, 0x79, 0x8D, 0x6E,
    0x1E, 0x4E, 0x98, 0x17, 0xE8, 0x89, 0x23, 0x91, 0x05, 0x6A, 0x11, 0x6D, 0x05, 0xB5, 0xC8, 0x4C,
    0x4B, 0x87, 0xFE, 0x85, 0xAD, 0xD8, 0xAA, 0x5D, 0xD5, 0x61, 0xA8, 0xE4, 0x76, 0xEC, 0x54, 0x55,
    0x43, 0x1A, 0x24, 0x31, 0xA9, 0xC2, 0xA9, 0xFC, 0xA4, 0xBF, 0x23, 0xF5, 0x39, 0xFC, 0x71, 0x25,
    0x1B, 0x93, 0x95, 0x13, 0x0F, 0xAA, 0xBE, 0x4E, 0x3A, 0xEA, 0xA8, 0x59, 0x85, 0xD5, 0x9F, 0x45,
    0x84, 0x61, 0x9F, 0x97, 0x01, 0xD3, 0xEC, 0xDF, 0xA2, 0xDD, 0x93, 0xBB, 0x28, 0x77, 0xE3, 0x3C,
    0x01, 0xDA, 0xEC, 0x54, 0xCF, 0x59, 0x68, 0x76, 0x01, 0x10, 0x66, 0x0B, 0xDB, 0x9F, 0x4C, 0xEC,
    0x6F, 0xC2, 0x06, 0xFD, 0xD1, 0xF6, 0x97, 0xF2, 0xCB, 0xCC, 0x79, 0x11, 0x3D, 0xBB, 0x0F, 0x27,
    0x10, 0x73, 0xC9, 0x46, 0xA2, 0x5A, 0x68, 0x8D, 0x6A, 0x11, 0xA9, 0xB8, 0x5A, 0x90, 0x42, 0x38,
    0xDC, 0x7E, 0x82, 0xA3, 0x33, 0x56, 0x60, 0x46, 0x8D, 0xF6, 0x92, 0x57, 0xAA, 0xD3, 0xDA, 0xC5,
    0x63, 0xA6, 0x93, 0x7E, 0x4F, 0xFC, 0x33, 0x13, 0x24, 0xB0, 0xFD, 0x9C, 0x7A, 0x5D, 0x70, 0x6A,
    0xC1, 0x7A, 0xB8, 0x3D, 0x79, 0x23, 0x16, 0xA7, 0x52, 0x91, 0x91, 0x9B, 0x03, 0x6D, 0x7C, 0x01,
    0xAB, 0xAF, 0xD4, 0x3A, 0x8A, 0xAC, 0x5E, 0xD3, 0xC8, 0xEA, 0x3C, 0xB2, 0xFD, 0x80, 0x31, 0x66,
    0x15, 0x00, 0x61, 0x96, 0xB0, 0xCB, 0x6C, 0x7F, 0x4C, 0xD8, 0xA0, 0xEF, 0x7F, 0xFE, 0xF4, 0xAB,
    0x7A, 0x23, 0x55, 0xF5, 0x52, 0x59, 0x5F, 0x84, 0x71, 0xD5, 0x9E, 0xFC, 0x64, 0x54, 0x8B, 0xB9,
    0x81, 0x9E, 0x38, 0x1C, 0x19, 0x82, 0xD7, 0x0A, 0xA9, 0x45, 0x5E, 0x7E, 0xE1, 0xF9, 0xBB, 0xE8,
    0x71, 0x6D, 0xE3, 0x7E, 0x2C, 0x69, 0xDE, 0x8F, 0xDB, 0xA1, 0x6E, 0x79, 0xDB, 0x41, 0xB8, 0xB5,
    0xFD, 0x08, 0x42, 0xDC, 0x7D, 0xA0, 0x44, 0x84, 0xA5, 0xA7, 0x12, 0xAB, 0x8B, 0x5E, 0x9D, 0x58,
    0xBD, 0xED, 0x93, 0x3B, 0xB2, 0x59, 0x9D, 0x35, 0x2B, 0x9C, 0x3D, 0x86, 0x61, 0x8C, 0x33, 0x03,
    0xC2, 0x2A, 0x61, 0x96, 0xB0, 0xCB, 0x64, 0x7F, 0x3E, 0x72, 0x4F, 0xB8, 0x64, 0x9F, 0x3F, 0xAA,
    0xDF, 0x63, 0x78, 0x11, 0xB4, 0x6B, 0x52, 0x29, 0xE0, 0x4B, 0x3C, 0x5B, 0x68, 0xB1, 0x21, 0xD4,
    0x4F, 0x54, 0x0B, 0xF7, 0x34, 0xBE, 0x6A, 0x41, 0x07, 0xEA, 0xAB, 0x1B, 0xF6, 0x11, 0x4B, 0xCB,
    0x06, 0xFD, 0xBC, 0x86, 0xDD, 0x70, 0x63, 0xF3, 0x41, 0x6A, 0xE7, 0x72, 0x67, 0xF5, 0x8B, 0x81,
    0xBE, 0x22, 0xAB, 0x0F, 0x7C, 0xFD, 0x44, 0x92, 0x57, 0x57, 0x2A, 0xBB, 0x74, 0x1B, 0xC2, 0xF4,
    0x99, 0x11, 0x56, 0x09, 0xB3, 0x45, 0xDD, 0xFF, 0x65, 0x19, 0x8A, 0xFD, 0xA6, 0xF9, 0xB1, 0xF4,
    0x04, 0xA6, 0x1F, 0xDA, 0x36, 0x65, 0x8E, 0x4C, 0xE0, 0xF2, 0x57, 0x8B, 0xAE, 0x1E, 0x4E, 0xD4,
    0x89, 0xE2, 0xA6, 0x16, 0xA4, 0x0C, 0xBA, 0xE7, 0xEF, 0xB0, 0x6B, 0xF8, 0x2C, 0xA6, 0x47, 0x38,
    0xE8, 0xCF, 0xEE, 0x1E, 0x39, 0x07, 0x86, 0x97, 0xB6, 0xF4, 0x6F, 0x58, 0x9C, 0xD5, 0x9F, 0x46,
    0x84, 0x62, 0xAF, 0x97, 0x1E, 0x53, 0xEC, 0xDF, 0x58, 0x85, 0xD5, 0x59, 0x53, 0x3D, 0x77, 0x11,
    0x53, 0x01, 0x10, 0x56, 0x25, 0x3C, 0xFC, 0xC6, 0x34, 0x14, 0xFB, 0xF6, 0xB3, 0xCF, 0x3E, 0xAB,
    0x3C, 0xEC, 0x4F, 0xE9, 0xAB, 0xD3, 0x5B, 0x2E, 0x5F, 0xC3, 0x09, 0xBC, 0xFC, 0xD5, 0x62, 0xA4,
    0x51, 0x2D, 0xAE, 0x5B, 0x50, 0x2D, 0x68, 0xF2, 0x79, 0x70, 0xD2, 0x12, 0x8C, 0x2D, 0xD1, 0x08,
    0x63, 0x8A, 0x35, 0x34, 0x99, 0xF4, 0x7B, 0x34, 0x88, 0x8A, 0xF0, 0x0B, 0xB2, 0x48, 0x07, 0xC6,
    0x2D, 0x3E, 0xB6, 0x80, 0xD5, 0xDD, 0x78, 0xB2, 0x3A, 0xFF, 0xAC, 0xB7, 0x6E, 0x3D, 0xCB, 0x06,
    0xE8, 0x48, 0xC2, 0xAA, 0x84, 0xCB, 0x2F, 0x4C, 0x36, 0xE8, 0x73, 0xBA, 0x51, 0xDF, 0xF9, 0xD7,
    0xCA, 0xBB, 0x24, 0x2F, 0x21, 0xDA, 0xB4, 0xD5, 0x4A, 0x20, 0xE6, 0xAF, 0x16, 0xAF, 0x98, 0xD4,
    0x82, 0x7F, 0x84, 0xA5, 0x89, 0xAC, 0x2E, 0x7A, 0xF5, 0xA9, 0xF6, 0x6F, 0xD0, 0x86, 0x3F, 0xAB,
    0x2B, 0x9A, 0x35, 0x77, 0x6C, 0x97, 0x5C, 0x00, 0x84, 0x51, 0xC2, 0x2A, 0x61, 0x56, 0x82, 0xFD,
    0x61, 0x9B, 0x09, 0xFC, 0xF4, 0xE5, 0xB7, 0x8D, 0xF2, 0xF7, 0xAB, 0x24, 0x48, 0x2A, 0x80, 0xCD,
    0xDB, 0xAC, 0x0F, 0x5E, 0x85, 0xD4, 0x62, 0xBD, 0x51, 0x2D, 0x0C, 0x72, 0xD4, 0x82, 0x81, 0xD5,
    0x2F, 0x18, 0x59, 0x7D, 0x00, 0x3B, 0xAB, 0xFF, 0xAB, 0x0B, 0x80, 0xB0, 0x49, 0x18, 0x65, 0xE8,
    0xFD, 0x33, 0xD9, 0x20, 0x1A, 0x27, 0x97, 0xEC, 0xF2, 0x6B, 0x95, 0xC3, 0xFF, 0x17, 0x40, 0xD1,
    0x6A, 0xD1, 0x57, 0x6B, 0x8F, 0xEE, 0x4E, 0x6F, 0x31, 0xD4, 0xD5, 0x0E, 0x93, 0x75, 0x8E, 0x98,
    0x6D, 0x70, 0x61, 0xCA, 0x49, 0x6E, 0x0E, 0x18, 0xEC, 0xF8, 0x16, 0x5D, 0xDF, 0xBF, 0x40, 0xCF,
    0xB7, 0x7F, 0xB5, 0x77, 0x96, 0x51, 0x6A, 0x64, 0x59, 0x1C, 0x3F, 0x1B, 0x20, 0x34, 0x34, 0x8D,
    0x53, 0xD0, 0x45, 0xE3, 0xD0, 0x0D, 0x1D, 0x77, 0x77, 0x59, 0x89, 0xDB, 0xB8, 0xBB, 0x4F, 0xDC,
    0xDD, 0x8D, 0x25, 0xEE, 0x09, 0x6D, 0x71, 0x77, 0x97, 0x71, 0xF7, 0x99, 0xAF, 0xEB, 0x6E, 0xE3,
    0x3E, 0x91, 0x7B, 0xF7, 0xDC, 0x0F, 0x6C, 0x9F, 0x6C, 0x8D, 0xA4, 0x0A, 0xA9, 0xBE, 0x75, 0xCE,
    0x2F, 0x8A, 0xD5, 0xE3, 0xFF, 0x2B, 0xEE, 0xAD, 0xF7, 0x8A, 0xBE, 0xF8, 0x03, 0x8E, 0xEA, 0x2C,
    0x00, 0x66, 0x93, 0x9A, 0x5F, 0x03, 0x65, 0x56, 0xB6, 0xED, 0x67, 0xD4, 0x50, 0x38, 0x4A, 0x8B,
    0x4A, 0xBA, 0x81, 0x69, 0x9F, 0xB1, 0x00, 0xD2, 0x54, 0xBC, 0x70, 0xE6, 0x6A, 0xF9, 0xC5, 0xE3,
    0xB2, 0x10, 0x3B, 0x7D, 0xF0, 0x7B, 0x42, 0xC2, 0x02, 0x60, 0x26, 0x31, 0x9B, 0x98, 0x51, 0x40,
    0x2B, 0x67, 0xF9, 0x53, 0xFF, 0x42, 0x99, 0xF0, 0x50, 0xA1, 0x69, 0x6D, 0x3E, 0x09, 0xC0, 0x02,
    0xB0, 0x00, 0x98, 0x49, 0xCC, 0xA6, 0xC4, 0x85, 0x2F, 0xB2, 0x7D, 0x0A, 0x38, 0xFD, 0x45, 0x96,
    0x3E, 0x30, 0xD9, 0xF0, 0xA5, 0xD4, 0x0B, 0xEA, 0x91, 0xCC, 0x8E, 0x00, 0x2C, 0x00, 0x0B, 0x80,
    0x59, 0xC4, 0x4C, 0x62, 0x36, 0x25, 0x8E, 0xFE, 0xB2, 0x7E, 0x0A, 0x44, 0x86, 0x0B, 0xCD, 0xF6,
    0xE6, 0x83, 0x00, 0x2C, 0x00, 0x0B, 0x80, 0x59, 0xC4, 0x4C, 0x4A, 0x1C, 0xFD, 0x65, 0xFF, 0x14,
    0x70, 0x45, 0x8A, 0x5D, 0xBF, 0xB8, 0xCF, 0xDB, 0xEE, 0xEB, 0x5C, 0x0A, 0xC0, 0x02, 0xB0, 0x00,
    0x98, 0x41, 0xCC, 0x22, 0x66, 0x52, 0xD9, 0xA3, 0x7F, 0xBD, 0x9F, 0x23, 0x00, 0x44, 0x87, 0x08,
    0x4D, 0x6A, 0x72, 0x29, 0x00, 0x0B, 0xC0, 0x02, 0x60, 0x06, 0x31, 0x8B, 0x12, 0xDF, 0xFB, 0xAF,
    0x5C, 0x2F, 0x60, 0xD7, 0x1A, 0x3A, 0xC1, 0xCC, 0xDB, 0x5F, 0xAF, 0xDB, 0x04, 0xFF, 0x7A, 0x05,
    0x0B, 0xC0, 0x02, 0x28, 0x2A, 0x00, 0x66, 0x0F, 0x33, 0xA8, 0x7C, 0xED, 0x2F, 0x31, 0x2F, 0xD0,
    0xC3, 0x16, 0x99, 0x98, 0x0B, 0x01, 0x58, 0x00, 0x16, 0x00, 0xB3, 0x27, 0x71, 0xDE, 0x5F, 0xF1,
    0x4D, 0x03, 0x58, 0x81, 0xA6, 0xB7, 0xB8, 0x5B, 0xBE, 0xC8, 0x02, 0xB0, 0x00, 0xD9, 0x14, 0x00,
    0x33, 0x87, 0xD9, 0xA3, 0x0C, 0x6A, 0x94, 0x8D, 0xBA, 0xF4, 0x1A, 0x21, 0xB1, 0xDC, 0x28, 0x0C,
    0xA5, 0xD3, 0xA2, 0x8A, 0x0B, 0xC0, 0x02, 0xB0, 0x00, 0x98, 0x35, 0xCC, 0x1C, 0x66, 0x4F, 0x7A,
    0xCD, 0x4F, 0x76, 0x4A, 0x21, 0x13, 0x10, 0x1B, 0xE4, 0x4A, 0x6C, 0xF9, 0x1F, 0x01, 0x52, 0xCA,
    0x08, 0xC0, 0x02, 0xB0, 0x00, 0x98, 0x35, 0xCC, 0x1C, 0x65, 0x2F, 0x8B, 0xA5, 0x8F, 0x44, 0x43,
    0x6C, 0xD0, 0x19, 0xDA, 0xDF, 0x55, 0xDA, 0xE6, 0x8F, 0x4A, 0x0A, 0xC0, 0x02, 0xB0, 0x00, 0x98,
    0x31, 0xCC, 0x9A, 0xEC, 0x8D, 0xAF, 0x0C, 0x0D, 0xB1, 0xBF, 0x93, 0x2D, 0x80, 0x3F, 0x59, 0xE6,
    0xB2, 0x42, 0x02, 0xB0, 0x00, 0x2C, 0xC0, 0x65, 0xCC, 0x18, 0x66, 0x2D, 0x57, 0x8D, 0xAF, 0xE4,
    0xB5, 0xC3, 0x40, 0xE5, 0x20, 0x67, 0x93, 0x1A, 0x25, 0x04, 0x60, 0x01, 0x58, 0x00, 0xCC, 0x16,
    0x66, 0x4C, 0xE2, 0x5A, 0xDF, 0x9C, 0x96, 0x42, 0x8D, 0x01, 0x41, 0xA7, 0xD3, 0xB5, 0xBD, 0xD5,
    0xD3, 0xEA, 0x1D, 0xFC, 0x02, 0x2C, 0x16, 0x40, 0x2E, 0x01, 0x58, 0x00, 0xCC, 0x14, 0x66, 0x0B,
    0x33, 0x26, 0xFD, 0x45, 0xB7, 0xB9, 0x2F, 0x85, 0x7C, 0x7E, 0xA3, 0x6D, 0x60, 0xD7, 0x65, 0xC9,
    0x0F, 0x59, 0x00, 0x16, 0x40, 0x0E, 0x5A, 0x24, 0x97, 0x7F, 0x88, 0x99, 0xC2, 0x6C, 0xE5, 0x4D,
    0xE9, 0x23, 0x51, 0x0A, 0x99, 0x80, 0x68, 0x64, 0xE4, 0xC8, 0x71, 0x89, 0xB7, 0x9F, 0xBF, 0xCC,
    0x02, 0xB0, 0x00, 0x37, 0xC8, 0x65, 0x1F, 0x64, 0x89, 0x96, 0x3B, 0x98, 0xF2, 0xA7, 0xF4, 0x91,
    0x3E, 0x2B, 0x64, 0x03, 0x9A, 0x86, 0x6A, 0x36, 0xD4, 0xB1, 0x00, 0x2C, 0xC0, 0x8D, 0x50, 0xB6,
    0x74, 0x5E, 0x1D, 0x4D, 0x78, 0xD9, 0xA4, 0xCF, 0xFA, 0xE4, 0x63, 0x3F, 0x60, 0x34, 0xB6, 0x2B,
    0x3F, 0x77, 0xF8, 0x5D, 0x16, 0xE0, 0xA7, 0xC0, 0x02, 0x84, 0xAA, 0x36, 0xBC, 0x8B, 0x19, 0x52,
    0xB0, 0xEE, 0x57, 0xBE, 0x1F, 0x28, 0xE9, 0xDD, 0x63, 0x50, 0xFC, 0xE5, 0x73, 0xFF, 0x66, 0x01,
    0x58, 0x80, 0x1F, 0x43, 0x78, 0x4F, 0xF5, 0xBF, 0x4B, 0x3A, 0xB6, 0x1B, 0xA4, 0x7C, 0xDD, 0xAF,
    0x7C, 0x3F, 0x10, 0xB1, 0xDF, 0x71, 0xF3, 0xBD, 0x89, 0xD7, 0x2F, 0x7E, 0xCA, 0x02, 0xFC, 0x20,
    0x58, 0x80, 0xFD, 0x75, 0x9F, 0x5A, 0x87, 0x0C, 0xBC, 0x17, 0xB3, 0xA3, 0x7C, 0xDD, 0xAF, 0x7C,
    0x3F, 0x60, 0x05, 0xE2, 0xEE, 0xA7, 0x1F, 0x79, 0x2A, 0xF1, 0xC6, 0xB3, 0x5F, 0xB3, 0x00, 0x2C,
    0x80, 0x24, 0x07, 0x76, 0x7C, 0xED, 0xB8, 0xFB, 0x96, 0xA7, 0x30, 0x33, 0x94, 0x1D, 0xE5, 0xEB,
    0x7E, 0x85, 0x25, 0xD0, 0x01, 0x76, 0xA0, 0xD2, 0x3B, 0x73, 0xF2, 0x8C, 0xC4, 0x9B, 0xCF, 0x7E,
    0xC7, 0x02, 0xB0, 0x00, 0xFF, 0x87, 0xEF, 0x84, 0x27, 0x1E, 0x9A, 0x81, 0x59, 0xA1, 0xCC, 0xE8,
    0x0A, 0x2B, 0xFC, 0xD2, 0x4D, 0xB1, 0x13, 0x68, 0xE6, 0x4B, 0x2E, 0x4C, 0xB2, 0x00, 0x2C, 0xC0,
    0xF5, 0x28, 0x9D, 0x32, 0x36, 0x89, 0x19, 0xA1, 0xAC, 0x50, 0xD3, 0xAB, 0x8E, 0xAD, 0x11, 0xED,
    0x90, 0x00, 0xB4, 0x0C, 0x6E, 0x59, 0x95, 0x66, 0x01, 0x32, 0x61, 0x01, 0xBC, 0x0B, 0x66, 0xA4,
    0x31, 0x1B, 0x94, 0x91, 0xC6, 0x99, 0x4D, 0xAF, 0x9A, 0x24, 0xD0, 0x03, 0x1E, 0xA0, 0x75, 0x68,
    0xD7, 0xB6, 0x43, 0x2C, 0x00, 0x0B, 0x80, 0xF8, 0x52, 0x8B, 0x0E, 0x61, 0x26, 0x28, 0x1B, 0x7A,
    0x55, 0x84, 0x5F, 0x42, 0x82, 0x22, 0x40, 0xC4, 0x75, 0x1D, 0x2A, 0x90, 0x80, 0x05, 0x90, 0x21,
    0xFC, 0xB4, 0xC6, 0x47, 0xA4, 0x6C, 0xA8, 0x28, 0xFC, 0xD2, 0x73, 0x04, 0x22, 0xD0, 0x36, 0xB8,
    0x79, 0x65, 0x15, 0x0B, 0xD0, 0x30, 0x05, 0xF0, 0xCE, 0x9F, 0x5E, 0x85, 0x19, 0x00, 0x44, 0xE9,
    0x73, 0xFD, 0xEA, 0x96, 0xA0, 0xB5, 0x6F, 0xD9, 0xFC, 0x94, 0xDA, 0xCE, 0x0E, 0xB1, 0x00, 0x92,
    0x7C, 0x57, 0x3A, 0x69, 0x74, 0x8A, 0xCA, 0x9E, 0x86, 0x11, 0x7E, 0x89, 0x72, 0xC8, 0x03, 0xB4,
    0x14, 0xA7, 0x4F, 0x9C, 0x25, 0x31, 0x4F, 0xC0, 0x02, 0xA8, 0xE8, 0x3C, 0xBF, 0xF0, 0xD8, 0x03,
    0xB3, 0xA8, 0xE1, 0xF5, 0xC8, 0x50, 0xF6, 0xA8, 0xA2, 0x31, 0x16, 0x80, 0x66, 0xEE, 0x27, 0x1F,
    0x1A, 0x9D, 0x78, 0xED, 0xC2, 0x67, 0x2C, 0x80, 0x3A, 0x05, 0x08, 0xEF, 0xAB, 0xFD, 0xCC, 0x7E,
    0xE7, 0xCD, 0xA3, 0xE9, 0x54, 0xA7, 0x20, 0x43, 0xC3, 0xAB, 0xAA, 0x53, 0xA4, 0x4E, 0xA0, 0xD2,
    0x7E, 0xDB, 0xA8, 0xFB, 0x0B, 0x78, 0xED, 0x10, 0x0B, 0x20, 0xB1, 0xB6, 0xC7, 0x32, 0x68, 0xC0,
    0xFD, 0x34, 0xC9, 0xE5, 0x54, 0xE0, 0x54, 0xA7, 0x2A, 0x26, 0xCB, 0xEC, 0x40, 0xDC, 0xD4, 0xBD,
    0xCB, 0xF0, 0xD8, 0xD9, 0x43, 0xEF, 0xB1, 0x00, 0xEA, 0x10, 0x20, 0x54, 0xB5, 0xFE, 0x3D, 0x53,
    0xBB, 0xD6, 0xC3, 0x69, 0x79, 0x83, 0x5D, 0x05, 0x93, 0x5C, 0x8A, 0x2E, 0x9B, 0xB0, 0x02, 0x11,
    0xAD, 0xC1, 0xD0, 0x35, 0x98, 0x5E, 0xB7, 0xAB, 0x30, 0x2F, 0xAA, 0x61, 0x01, 0x88, 0xCB, 0xDE,
    0x25, 0x73, 0x76, 0xE1, 0x7B, 0x49, 0x0B, 0xDB, 0xAC, 0xEA, 0x58, 0xDE, 0xA0, 0xFC, 0x02, 0x3A,
    0x13, 0x2D, 0x83, 0x6D, 0xEB, 0x1E, 0xF7, 0xC4, 0xE4, 0xC4, 0x2B, 0xE7, 0x3F, 0x64, 0x01, 0x0A,
    0xAC, 0xE4, 0xD9, 0x5B, 0xF3, 0xA1, 0xEB, 0xBE, 0xBB, 0x26, 0xD3, 0x69, 0x4E, 0x1F, 0x60, 0xFA,
    0xE1, 0x0B, 0xDB, 0x58, 0x02, 0x0D, 0x60, 0xA0, 0x46, 0xA9, 0xA9, 0xA9, 0x73, 0x87, 0x11, 0xE5,
    0x67, 0x0E, 0xBE, 0xCB, 0x02, 0x14, 0x06, 0xC1, 0x6D, 0xEB, 0xDF, 0x35, 0xB5, 0x6A, 0x35, 0x82,
    0xAE, 0xE4, 0x12, 0xE8, 0xBD, 0xD4, 0xFC, 0xB4, 0xF0, 0x73, 0x73, 0x6C, 0x03, 0xA2, 0xFF, 0x2D,
    0x89, 0xB6, 0xAD, 0xDD, 0x91, 0xD7, 0x25, 0x11, 0x0B, 0x70, 0xB9, 0x6C, 0xE1, 0xEC, 0x1D, 0x54,
    0xF2, 0x44, 0x01, 0x1B, 0x37, 0xBB, 0xF2, 0x95, 0x44, 0x65, 0x40, 0x1B, 0xE1, 0xA9, 0x87, 0xC7,
    0x57, 0x3C, 0x77, 0xF2, 0x4F, 0x2C, 0x40, 0x7E, 0x11, 0xDE, 0xB1, 0xED, 0x4F, 0xCE, 0xBB, 0x6F,
    0x19, 0x8F, 0xEF, 0x11, 0x50, 0x26, 0x6F, 0xC9, 0xC3, 0x12, 0x34, 0xCA, 0x28, 0x89, 0x2A, 0xB5,
    0x0E, 0x4B, 0xEF, 0xC0, 0xC6, 0x54, 0x4D, 0xE2, 0x8D, 0x4B, 0x5F, 0xB1, 0x00, 0x39, 0xE6, 0xC0,
    0xF6, 0xAF, 0xBC, 0xF3, 0xA6, 0xD5, 0x68, 0x2D, 0x96, 0xDE, 0xF8, 0xDE, 0x64, 0x94, 0x3C, 0x8D,
    0x38, 0xFC, 0xCA, 0x94, 0x44, 0x16, 0xC0, 0x0F, 0xB4, 0x36, 0xF7, 0xE9, 0x79, 0x7B, 0xF4, 0xD8,
    0x9E, 0x57, 0x58, 0x80, 0xDC, 0xE0, 0xDF, 0xB4, 0xF2, 0x95, 0xE2, 0x2E, 0xED, 0x6E, 0xA7, 0x25,
    0x0D, 0x7E, 0xC0, 0xC2, 0x25, 0x4F, 0xF6, 0x1A, 0x64, 0x27, 0x10, 0x03, 0x3A, 0x7B, 0x26, 0x8F,
    0x9E, 0x15, 0x7F, 0xE1, 0xCC, 0xDF, 0x58, 0x80, 0xEC, 0x10, 0xDE, 0x99, 0xFE, 0x9B, 0xF0, 0xD0,
    0x3D, 0xB3, 0x70, 0xEC, 0xE9, 0x3D, 0x70, 0x66, 0xBF, 0xD1, 0xE5, 0x4F, 0x03, 0x1D, 0xD5, 0x99,
    0x22, 0xD0, 0x54, 0x2F, 0x8A, 0xFD, 0xA1, 0x49, 0xDE, 0x05, 0x8B, 0xEA, 0xBE, 0x66, 0x01, 0x14,
    0xE2, 0xE0, 0x8E, 0xAF, 0xBD, 0x8B, 0x66, 0xEF, 0xD2, 0x0B, 0x42, 0x7F, 0x3A, 0xC3, 0x23, 0xD2,
    0x7B, 0xA0, 0xE3, 0xA3, 0x7E, 0xEE, 0x7A, 0x03, 0x3D, 0x4D, 0xB0, 0x04, 0x81, 0x36, 0xE6, 0x6E,
    0x9D, 0x6F, 0x09, 0xD5, 0x6D, 0x39, 0x2C, 0x53, 0x7F, 0xC0, 0x02, 0x50, 0x9D, 0xEF, 0x5B, 0xBE,
    0xE0, 0xB0, 0xA9, 0x6D, 0xAB, 0x5B, 0xA8, 0xC9, 0x0D, 0xD2, 0x98, 0xEB, 0xB9, 0xD6, 0xCF, 0xBF,
    0xB2, 0x28, 0x0A, 0xB4, 0x33, 0xB5, 0x6A, 0x3E, 0x02, 0x66, 0x92, 0xF7, 0xC0, 0xD7, 0xB1, 0x7C,
    0xC6, 0x02, 0xFC, 0xD4, 0xC5, 0x6B, 0x75, 0x9F, 0x79, 0x17, 0xCD, 0xD9, 0x63, 0x68, 0x12, 0x1F,
    0x81, 0x63, 0x0A, 0x44, 0xB9, 0xDC, 0xC9, 0xEF, 0xB2, 0x48, 0x0B, 0x14, 0x03, 0x2E, 0x9A, 0x7A,
    0x6F, 0x6B, 0xAC, 0x88, 0x0E, 0x0E, 0x6C, 0x5A, 0x59, 0x17, 0x7F, 0xE5, 0xFC, 0xC7, 0x2C, 0xC0,
    0x0F, 0x64, 0x6F, 0xED, 0xC7, 0xE2, 0xBC, 0xE9, 0x75, 0xFA, 0x50, 0x68, 0x30, 0x8E, 0x21, 0x8D,
    0xA5, 0x8B, 0xC6, 0x56, 0xCB, 0xE5, 0x4E, 0xFE, 0x97, 0x45, 0x5A, 0xC0, 0x48, 0x47, 0xAB, 0x30,
    0xD0, 0x5A, 0x1F, 0xF0, 0x0E, 0x08, 0xAC, 0x59, 0xBE, 0x35, 0xFE, 0xC2, 0xD9, 0x7F, 0xB2, 0x00,
    0xD7, 0x27, 0xBC, 0xAB, 0xEA, 0x9F, 0xE2, 0xAC, 0xC9, 0x5B, 0xF5, 0x1E, 0xCF, 0x00, 0x1C, 0x33,
    0x1A, 0x3B, 0x27, 0x8D, 0xA5, 0xB6, 0x70, 0xCB, 0x1D, 0x16, 0xC1, 0x01, 0x84, 0x80, 0x56, 0x5A,
    0xAD, 0xB6, 0xA7, 0xFB, 0xA9, 0x87, 0xA7, 0x44, 0xF6, 0xD5, 0x5C, 0x88, 0xBF, 0x76, 0xE9, 0x8B,
    0x06, 0x2F, 0xC0, 0xFE, 0xBA, 0x2F, 0xFC, 0xAB, 0x96, 0x5E, 0x70, 0xDC, 0x7D, 0xEB, 0x14, 0x1C,
    0x1B, 0x1C, 0x23, 0x1A, 0x2B, 0x87, 0xBA, 0x82, 0xCF, 0x22, 0x18, 0x68, 0x29, 0xAE, 0x8F, 0x26,
    0x6C, 0x3A, 0xEA, 0xBD, 0x9E, 0x01, 0xDE, 0xF9, 0x33, 0x92, 0xB1, 0x13, 0xFB, 0xDE, 0x82, 0xD0,
    0x5E, 0x6E, 0x30, 0x02, 0x1C, 0xDC, 0x79, 0x39, 0xB0, 0x79, 0xCD, 0x5B, 0x9E, 0xD1, 0x8F, 0x27,
    0xF5, 0x1E, 0xE7, 0x00, 0x1C, 0x0B, 0x1A, 0x13, 0x1F, 0x8D, 0x91, 0x41, 0xBD, 0xC1, 0xE7, 0x66,
    0x59, 0x0F, 0x94, 0xD0, 0x8C, 0x65, 0x10, 0x68, 0x0E, 0x74, 0x2D, 0xEE, 0xD4, 0xFE, 0x56, 0xFF,
    0xDA, 0x64, 0xBA, 0xE2, 0xD2, 0xF1, 0xDF, 0xAA, 0x55, 0x80, 0x50, 0xDD, 0xA6, 0xDF, 0x42, 0x89,
    0x93, 0x2E, 0x6E, 0xD5, 0xFC, 0x56, 0xDC, 0x67, 0xDA, 0xF7, 0x20, 0x20, 0xD0, 0x98, 0xE8, 0xB9,
    0xB9, 0x6D, 0x58, 0x9F, 0x0A, 0x45, 0x80, 0x85, 0xAE, 0x4D, 0x8D, 0xD2, 0xC7, 0x7F, 0x77, 0xB8,
    0x78, 0xE3, 0x66, 0x71, 0xEE, 0xB4, 0x65, 0xE1, 0x3D, 0x55, 0x67, 0xE3, 0xCF, 0x9F, 0xFE, 0x6B,
    0xA1, 0x0A, 0x10, 0xDE, 0x95, 0xFE, 0xAB, 0x6F, 0xE5, 0xD2, 0xB3, 0xC2, 0xE8, 0x47, 0x97, 0x99,
    0x5A, 0x34, 0xBD, 0x19, 0xF7, 0x8D, 0xF6, 0x31, 0x4A, 0xFB, 0x6C, 0x01, 0x8A, 0xF8, 0x68, 0xCF,
    0x32, 0xE8, 0xA8, 0xDE, 0xB5, 0xD1, 0xE4, 0x4E, 0x84, 0xAE, 0x5B, 0xED, 0x00, 0xF4, 0x32, 0xF7,
    0xEA, 0x7E, 0x57, 0xD9, 0xA2, 0x59, 0x2B, 0x23, 0xFB, 0x6B, 0x2F, 0xC5, 0x5F, 0x3A, 0xFB, 0xAF,
    0xBC, 0x15, 0x60, 0x77, 0xF5, 0xBF, 0x7C, 0xAB, 0x97, 0x5D, 0x72, 0x8F, 0x7D, 0x72, 0x65, 0x71,
    0xA7, 0xB6, 0x77, 0xE1, 0x6B, 0xA7, 0x7D, 0x68, 0x46, 0xFB, 0x24, 0x02, 0x36, 0xDA, 0x57, 0x1D,
    0x87, 0xBE, 0xFE, 0xC6, 0x22, 0x68, 0x68, 0x2D, 0x8B, 0x81, 0x8E, 0x8E, 0x2E, 0xA0, 0x8C, 0x8E,
    0x98, 0xCD, 0xA9, 0x56, 0xEE, 0x6D, 0xED, 0xD7, 0xFB, 0x1E, 0x58, 0x7E, 0x31, 0xD7, 0xBF, 0x66,
    0x79, 0x3A, 0xBC, 0xBB, 0xEA, 0x2C, 0x5C, 0xBE, 0xF9, 0x41, 0xC5, 0xCB, 0xE7, 0x3E, 0xCC, 0x86,
    0x00, 0x74, 0x7D, 0xED, 0x87, 0xB0, 0xE6, 0xFE, 0x03, 0x5F, 0x6A, 0xF1, 0x59, 0x71, 0xC6, 0xE4,
    0x34, 0x2C, 0x4B, 0x98, 0x5B, 0xD2, 0xAD, 0xE3, 0x3D, 0xF8, 0xDA, 0xE8, 0x35, 0x36, 0x07, 0xA2,
    0xF4, 0xDA, 0x5D, 0xB4, 0x2F, 0x06, 0xDA, 0x37, 0x0D, 0x07, 0x5F, 0x7A, 0x63, 0x19, 0x32, 0xCA,
    0x24, 0x0A, 0x8D, 0x91, 0x42, 0x24, 0x64, 0x08, 0x51, 0x49, 0xE5, 0x44, 0x07, 0x2A, 0x2D, 0xFA,
    0x1A, 0x62, 0xE1, 0xE1, 0xCE, 0x7B, 0x6F, 0x1B, 0x03, 0x8D, 0x75, 0xCA, 0xBF, 0x2E, 0xB5, 0x27,
    0xB8, 0x65, 0xCD, 0x19, 0x98, 0x95, 0x7E, 0x39, 0xB2, 0xAF, 0xF6, 0x83, 0xE8, 0x91, 0xDD, 0x7F,
    0x88, 0x9D, 0x3A, 0xF0, 0xAF, 0xD8, 0xE9, 0x43, 0x1F, 0x97, 0x9F, 0x3B, 0xF2, 0x59, 0xF9, 0xF9,
    0xA3, 0x5F, 0x97, 0x5F, 0x38, 0x76, 0x19, 0xC1, 0x3F, 0xE3, 0xBF, 0xE1, 0xFF, 0xE1, 0x6D, 0xF0,
    0xB6, 0x78, 0x1F, 0xBC, 0x2F, 0x3E, 0x86, 0x2F, 0xB9, 0x70, 0x8F, 0x7B, 0xF4, 0xE3, 0x29, 0xFB,
    0xB0, 0x21, 0x63, 0x1A, 0x07, 0x02, 0xC3, 0xF1, 0xB9, 0xE8, 0x39, 0x3B, 0xD0, 0x6B, 0xA8, 0xCC,
    0x08, 0xBC, 0x40, 0xAF, 0xD5, 0x08, 0x34, 0xCE, 0x2C, 0x6F, 0x38, 0xF4, 0x72, 0x6D, 0x2C, 0x84,
    0x81, 0x9A, 0x46, 0x1B, 0x1D, 0x61, 0x4B, 0x01, 0x1F, 0x9D, 0x37, 0x8F, 0x53, 0xC9, 0xD1, 0x86,
    0x02, 0xDA, 0x99, 0xC2, 0xDA, 0x13, 0xE8, 0x43, 0xE1, 0xED, 0x07, 0xF4, 0x07, 0x7E, 0x4E, 0xF4,
    0xA7, 0x7F, 0xEB, 0x4B, 0xB7, 0xE9, 0x49, 0xF7, 0xE9, 0x4C, 0x8F, 0xD1, 0x86, 0x1E, 0x33, 0x4E,
    0xCF, 0xE1, 0xA3, 0xE7, 0x74, 0x01, 0x36, 0x7A, 0x2D, 0x06, 0x0E, 0x7C, 0xB6, 0x37, 0x16, 0x42,
    0x43, 0x81, 0xD3, 0x51, 0xF8, 0x8A, 0x80, 0x62, 0x3A, 0x02, 0xDB, 0x29, 0xA0, 0x6E, 0x6A, 0x38,
    0x45, 0x3A, 0x42, 0xFB, 0x08, 0x7F, 0x3D, 0x7C, 0x44, 0x19, 0xDD, 0xD6, 0x43, 0xF7, 0x75, 0xD1,
    0x63, 0x59, 0xE8, 0xB1, 0x8B, 0xE8, 0xB9, 0x74, 0xF4, 0xDC, 0x9A, 0xFC, 0x0A, 0x3C, 0x6F, 0xDC,
    0x4B, 0x64, 0xC8, 0x41, 0x90, 0x24, 0x24, 0x8A, 0x34, 0x3A, 0x42, 0x9B, 0x01, 0x85, 0x9C, 0x6B,
    0x77, 0xDE, 0x78, 0xE3, 0x4D, 0xC6, 0xED, 0x1A, 0x75, 0xF9, 0x25, 0xBB, 0x04, 0x8A, 0x27, 0x9D,
    0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};
static std::wstring g_LibbyIconTempPath;

// Classifies the SMTC session's origin so feature code can gate on session type
// without re-parsing the AUMID string every frame.
enum class SessionSource {
    Unknown = 0,   // initial zero-initialized state; classification not yet run
    NativeApp,     // Win32 or Store app (Spotify, Libby, WMP, …)
    Browser,       // Chromium-family browser (Chrome, Edge, Brave, Opera, Vivaldi, …)
    BrowserFirefox, // Firefox — never exposes timeline data (Mozilla bug 1689538)
};

struct MediaState {
    std::wstring title;
    std::wstring artist;
    std::wstring sessionId;    // AUMID (SourceAppUserModelId)
    bool isPlaying = false;
    double playbackRate = 1.0;  // listen speed; 1.0 = normal
    bool canSkipForward  = false; // SMTC Controls.IsNextEnabled — next track/chapter
    bool canSkipBackward = false; // SMTC Controls.IsPreviousEnabled — previous track/chapter
    int64_t positionMs = 0;
    int64_t durationMs = 0;
    bool isAudiobook = false;          // true when durationMs > 1 hour
    std::wstring positionFormatted;    // "MM:SS" or "HH:MM:SS" — updated with timeline
    GlobalSystemMediaTransportControlsSession session{ nullptr };
    event_token propsChangedToken{};
    event_token playbackChangedToken{};
    event_token timelineChangedToken{};
    IRandomAccessStreamReference thumbnailRef{ nullptr };  // null = no art (Libby, etc.)
    uint32_t thumbnailVersion = 0;                         // incremented each time art changes
    SessionSource source = SessionSource::Unknown;         // set at enumeration time
};

static MediaState g_MediaStates[MAX_SESSIONS];
static int g_MediaStateCount = 0;
static int g_ActiveSessionIndex = 0;
static std::mutex g_MediaMutex;
static GlobalSystemMediaTransportControlsSessionManager g_SessionManager{ nullptr };
static event_token g_SessionsChangedToken{};
static IAsyncOperation<GlobalSystemMediaTransportControlsSessionManager> g_PendingRequest{ nullptr };
static std::atomic<int> g_AsyncTasks{ 0 };
static std::atomic<HANDLE> g_GsmtcStartEvent{ nullptr };
static HANDLE g_GsmtcThread = nullptr;
static DWORD g_GsmtcThreadId = 0;

// ---------- XAML injection state ----------
constexpr std::wstring_view kWidgetRootName = L"TaskbarMediaWidgetRoot";
constexpr std::wstring_view kTitleName      = L"NowPlayingTitle";
constexpr std::wstring_view kArtistName     = L"NowPlayingArtist";
constexpr std::wstring_view kPlayPauseName  = L"NowPlayingPlayPause";
constexpr std::wstring_view kSkipBackName   = L"NowPlayingSkipBack";
constexpr std::wstring_view kSkipFwdName    = L"NowPlayingSkipFwd";
constexpr std::wstring_view kSessionCountName = L"NowPlayingSessionCount";
constexpr std::wstring_view kAlbumArtName     = L"NowPlayingAlbumArt";
constexpr std::wstring_view kProgressTrackName = L"NowPlayingProgressTrack";
constexpr std::wstring_view kProgressFillName  = L"NowPlayingProgressFill";
constexpr std::wstring_view kTimestampName     = L"NowPlayingTimestamp";
constexpr std::wstring_view kTitleCanvasName    = L"NowPlayingTitleCanvas";
constexpr std::wstring_view kTitleScrollerName  = L"NowPlayingTitleScroller";
constexpr std::wstring_view kTitleName2         = L"NowPlayingTitle2";
constexpr std::wstring_view kTaskbarFrameClass  = L"Taskbar.TaskbarFrame";
constexpr std::wstring_view kRootGridName       = L"RootGrid";
constexpr std::wstring_view kSystemTrayGridName = L"SystemTrayFrameGrid";

static std::mutex g_WidgetMutex;
static weak_ref<Grid> g_WidgetRoot{ nullptr };
static weak_ref<Grid> g_RootGrid{ nullptr };
static weak_ref<FrameworkElement> g_SystemTray{ nullptr };
static event_token g_TrayResizeToken{};
static std::atomic<bool> g_ScanPending{ false };
static std::atomic<bool> g_TaskbarViewDllLoaded{ false };
static std::atomic<int> g_HookCallCounter{ 0 };
static std::atomic<bool> g_Unloading{ false };
static Storyboard           g_MarqueeStoryboard{ nullptr };
static Storyboard           g_TextFadeStoryboard{ nullptr };
static Storyboard           g_WidgetFadeStoryboard{ nullptr };
static bool                 g_WidgetFadeTargetVisible = false;
static winrt::event_token   g_TitleSizeChangedToken{};
static HANDLE g_PollThread = nullptr;
static HANDLE g_PollStop = nullptr;
static std::thread g_PollForDllThread;
static std::thread g_InitialScanThread;
static std::atomic<HWND> g_hTaskbarWnd{ nullptr };
static DispatcherTimer      g_ProgressTimer{ nullptr };
static ULONGLONG            g_ProgressLastTickMs = 0;

// ---------- SC-FLY-1: flyout HWND (Win32 WS_POPUP on dedicated thread) ----------
// WUX Popup is non-functional in the taskbar XAML Island (probe confirmed: IsOpen=true
// fires but the compositor target has no visible output). A separate Win32 window is used.

#define WM_FLYOUT_SHOW         (WM_APP + 20)  // show and position above widget
#define WM_FLYOUT_HIDE_DELAYED (WM_APP + 21)  // start 300ms hide timer
#define WM_FLYOUT_HIDE_NOW     (WM_APP + 22)  // cancel timer, hide immediately
#define WM_FLYOUT_UPDATE       (WM_APP + 23)  // invalidate content (title/artist changed)
#define WM_FLYOUT_QUIT         (WM_APP + 24)  // destroy HWND and exit thread message loop
#define WM_FLYOUT_SETTINGS     (WM_APP + 25)  // re-apply alpha after settings change

static std::atomic<HWND> g_FlyoutHwnd{ nullptr };
static HANDLE g_FlyoutThread   = nullptr;
static DWORD  g_FlyoutThreadId = 0;

// Flyout content strings — written on the XAML UI thread, read on the flyout thread.
static std::mutex   g_FlyoutContentMutex;
static std::wstring g_FlyoutTitleStr;
static std::wstring g_FlyoutArtistStr;

// Flyout album art — decoded to HBITMAP once per track on the XAML thread,
// drawn via StretchBlt on the flyout thread. Mutex prevents torn reads/writes.
static std::mutex g_FlyoutArtMutex;
static HBITMAP    g_FlyoutArtHBitmap = nullptr;
static int        g_FlyoutArtBmpW    = 0;
static int        g_FlyoutArtBmpH    = 0;

// Right-mode only: trayWidth + offsetX (DIPs). Written by UpdateWidgetMargin(),
// read by the flyout thread to compute the widget's left screen-X in Right mode.
static std::atomic<int> g_FlyoutMarginDIPs{ 0 };

// ---------- Helpers ----------
// Cached vtable slot index — avoids rescanning every hook call once found.
static std::atomic<int> g_FrameworkElementSlot{ -1 };

static bool SlotHasVtablePointer(void* candidate) {
    // Read the pointer stored at this slot (safe — candidate is within the live
    // object). Require it to be committed MEM_IMAGE memory: vtables always live
    // in a module's .rdata section. Data members (refcount ≈ 2, heap pointers)
    // point into MEM_FREE or MEM_PRIVATE and are rejected before any QI call.
    void* vtable = *reinterpret_cast<void**>(candidate);
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(vtable, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Type  != MEM_IMAGE)  return false;
    constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE |
                                PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                                PAGE_EXECUTE_WRITECOPY;
    return (mbi.Protect & kReadable) && !(mbi.Protect & PAGE_GUARD);
}

static FrameworkElement GetFrameworkElementFromNative(void* pThis) {
    auto trySlot = [&](int slot) -> FrameworkElement {
        void* candidate = static_cast<char*>(pThis) + slot * sizeof(void*);
        void* vtable    = *reinterpret_cast<void**>(candidate);
        if (!SlotHasVtablePointer(candidate)) {
            return nullptr;
        }
        try {
            FrameworkElement fe{ nullptr };
            HRESULT hr = reinterpret_cast<::IUnknown*>(candidate)->QueryInterface(
                winrt::guid_of<FrameworkElement>(), winrt::put_abi(fe));
            return (SUCCEEDED(hr) && fe) ? fe : nullptr;
        } catch (...) {
            return nullptr;
        }
    };

    // Fast path: reuse cached slot.
    int cached = g_FrameworkElementSlot.load(std::memory_order_relaxed);
    if (cached >= 0) {
        if (auto fe = trySlot(cached)) return fe;
        g_FrameworkElementSlot.store(-1, std::memory_order_relaxed);
    }

    // Slow path: scan and cache.
    for (int slot = 0; slot <= 8; ++slot) {
        if (auto fe = trySlot(slot)) {
            g_FrameworkElementSlot.store(slot, std::memory_order_relaxed);
            return fe;
        }
    }
    return nullptr;
}

static FrameworkElement WalkUpToTaskbarFrame(FrameworkElement start) {
    FrameworkElement cur = start;
    while (cur) {
        if (winrt::get_class_name(cur) == kTaskbarFrameClass) return cur;
        auto parent = VisualTreeHelper::GetParent(cur);
        cur = parent ? parent.try_as<FrameworkElement>() : nullptr;
    }
    return nullptr;
}

static Grid FindRootGrid(FrameworkElement taskbarFrame) {
    if (!taskbarFrame) return nullptr;
    int count = VisualTreeHelper::GetChildrenCount(taskbarFrame);
    for (int i = 0; i < count; ++i) {
        auto child = VisualTreeHelper::GetChild(taskbarFrame, i);
        if (!child) continue;
        auto fe = child.try_as<FrameworkElement>();
        if (fe && std::wstring(fe.Name()) == kRootGridName)
            return fe.try_as<Grid>();
    }
    return nullptr;
}

// Returns true when the taskbar/shell should use a light background.
// Checks SystemUsesLightTheme (controls the taskbar itself) first, then falls
// back to AppsUseLightTheme — whichever indicates light wins, so the widget's
// Acrylic HostBackdrop (which follows the taskbar) and its text color agree.
static bool IsSystemLightTheme() {
    DWORD sysVal = 0, sysSize = sizeof(sysVal);
    LSTATUS sySt = RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &sysVal, &sysSize);
    DWORD appVal = 0, appSize = sizeof(appVal);
    LSTATUS appSt = RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &appVal, &appSize);
    return (sySt == ERROR_SUCCESS && sysVal != 0) || (appSt == ERROR_SUCCESS && appVal != 0);
}

static SolidColorBrush MakeBrush(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return SolidColorBrush(ColorHelper::FromArgb(a, r, g, b));
}

// Forward
static void RefreshWidgetUI();
static void StartMarqueeIfNeeded(Canvas titleCanvas, TextBlock titleTb);

// Recompute the widget's margin based on the current position mode.
// Right: tracks tray width so the widget stays adjacent to the system tray.
// Left/Center: a fixed margin from the respective edge (no tray dependency).
// Called at inject time, on tray resize (Right mode only matters, but harmless
// for other modes), and whenever settings change.
static void UpdateWidgetMargin() {
    Grid widget{ nullptr };
    FrameworkElement tray{ nullptr };
    {
        std::lock_guard<std::mutex> g(g_WidgetMutex);
        widget = g_WidgetRoot.get();
        tray   = g_SystemTray.get();
    }
    if (!widget) return;

    double gap = (double)g_Settings.offsetX;
    if (g_Settings.widgetPosition == WidgetPosition::Right) {
        double trayWidth = tray ? tray.ActualWidth() : 0.0;
        double margin    = trayWidth + gap;
        widget.Margin(ThicknessHelper::FromLengths(0, 0, margin, 0));
        g_FlyoutMarginDIPs.store((int)margin);
    } else {
        // Left: Margin.Left is a gap from the left edge.
        // Center: HorizontalAlignment::Center centers the widget; Margin.Left
        //         nudges it rightward from that center point when offsetX > 0.
        widget.Margin(ThicknessHelper::FromLengths(gap, 0, 0, 0));
    }
}

// ---------- SC-M-2: BringSourceAppToFront — Messij ----------

static std::wstring ExtractExeHint(const std::wstring& aumid) {
    // Classic AUMID: "Spotify.exe!App" — pre-bang ends with .exe → strip it.
    // Store AUMID:   "SpotifyAB.SpotifyMusic_zpdnekdrzrea0!Spotify" — pre-bang
    //   does not end with .exe; use the post-bang AppId ("Spotify") as the
    //   exe hint instead, which matches the actual Spotify.exe process name.
    auto bang = aumid.find(L'!');
    std::wstring pre = (bang != std::wstring::npos) ? aumid.substr(0, bang) : aumid;
    std::wstring lower_pre = pre;
    for (auto& c : lower_pre) c = (wchar_t)towlower(c);
    if (lower_pre.size() >= 4 &&
        lower_pre.substr(lower_pre.size() - 4) == L".exe")
        return lower_pre.substr(0, lower_pre.size() - 4);
    // Store AUMID: the AppId after '!' is usually the process base name.
    if (bang != std::wstring::npos) {
        std::wstring post = aumid.substr(bang + 1);
        for (auto& c : post) c = (wchar_t)towlower(c);
        if (post.size() >= 4 && post.substr(post.size() - 4) == L".exe")
            post = post.substr(0, post.size() - 4);
        // Generic sentinel AppId — the package family name in pre-bang is more
        // useful. Map known package-family substrings to their exe stems so that
        // ClassifySessionSource and BringSourceAppToFront both work correctly.
        // Example: "Microsoft.MicrosoftEdge.Stable_8wekyb3d8bbwe!App" → "msedge".
        if (post == L"app") {
            if (lower_pre.find(L"microsoftedge") != std::wstring::npos) return L"msedge";
            return lower_pre;  // unknown Store app; ClassifySessionSource handles it
        }
        return post;
    }
    // Chrome extension / Chrome App AUMID: "Chrome._crx_<id>" or "Chrome.<name>".
    // No '!' and no '.exe', but the stem before the first dot is the browser name.
    auto dot = lower_pre.find(L'.');
    if (dot != std::wstring::npos && dot > 0)
        return lower_pre.substr(0, dot);
    return lower_pre;
}

// Classify a session's AUMID into a SessionSource bucket.
// Calls ExtractExeHint() to obtain the lowercased exe stem, then matches against
// known browser exe names.  Returns NativeApp for everything else.
// BrowserFirefox is kept separate because Firefox never exposes timeline data
// (Mozilla Bugzilla 1689538), which may be useful for feature gating in Task D.
static SessionSource ClassifySessionSource(const std::wstring& aumid) {
    std::wstring stem = ExtractExeHint(aumid);  // already lowercased, .exe stripped
    if (stem == L"firefox") return SessionSource::BrowserFirefox;
    static const std::wstring kBrowserStems[] = {
        L"chrome", L"msedge", L"brave", L"opera", L"operagx",
        L"vivaldi", L"arc", L"thorium", L"chromium",
    };
    for (const auto& b : kBrowserStems) {
        if (stem == b) return SessionSource::Browser;
    }
    // Fallback for Store AUMIDs with a generic AppId: ExtractExeHint returns the
    // package family name. Check for known browser package-name substrings.
    if (stem.find(L"microsoftedge") != std::wstring::npos) return SessionSource::Browser;
    return SessionSource::NativeApp;
}

struct FindWindowCtx {
    std::wstring aumid;
    std::wstring hint;
    HWND best = nullptr;
    int  score = 0;
};

static BOOL CALLBACK FindWindowByAppIdProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) return TRUE;
    auto* ctx = reinterpret_cast<FindWindowCtx*>(lParam);

    // Primary: match via Shell property store PKEY_AppUserModel_ID
    IPropertyStore* store = nullptr;
    if (SUCCEEDED(SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&store))) && store) {
        PROPVARIANT pv;
        PropVariantInit(&pv);
        if (SUCCEEDED(store->GetValue(kPKEY_AppUserModel_ID, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal) {
            std::wstring id(pv.pwszVal);
            PropVariantClear(&pv);
            store->Release();
            if (id == ctx->aumid) {
                ctx->best  = hwnd;
                ctx->score = 100;
                return FALSE;  // exact match — stop enumeration
            }
        } else {
            PropVariantClear(&pv);
            store->Release();
        }
    }

    // Fallback: exe-name hint match
    if (ctx->score < 100 && !ctx->hint.empty()) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid) {
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (hProc) {
                wchar_t path[MAX_PATH] = {};
                DWORD len = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, path, &len) && len > 0) {
                    std::wstring exePath(path, len);
                    auto slash = exePath.rfind(L'\\');
                    std::wstring exeName = (slash != std::wstring::npos) ? exePath.substr(slash + 1) : exePath;
                    auto dot = exeName.rfind(L'.');
                    if (dot != std::wstring::npos) exeName = exeName.substr(0, dot);
                    for (auto& c : exeName) c = (wchar_t)towlower(c);
                    if (exeName == ctx->hint && ctx->score < 50) {
                        ctx->best  = hwnd;
                        ctx->score = 50;
                    }
                }
                CloseHandle(hProc);
            }
        }
    }
    return TRUE;
}

static BOOL CALLBACK FindBestWindowProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) return TRUE;
    auto* ctx = reinterpret_cast<FindWindowCtx*>(lParam);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return TRUE;
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return TRUE;
    wchar_t path[MAX_PATH] = {};
    DWORD len = MAX_PATH;
    if (QueryFullProcessImageNameW(hProc, 0, path, &len) && len > 0) {
        std::wstring exePath(path, len);
        auto slash = exePath.rfind(L'\\');
        std::wstring exeName = (slash != std::wstring::npos) ? exePath.substr(slash + 1) : exePath;
        auto dot = exeName.rfind(L'.');
        if (dot != std::wstring::npos) exeName = exeName.substr(0, dot);
        for (auto& c : exeName) c = (wchar_t)towlower(c);
        if (exeName == ctx->hint) {
            RECT rc{};
            GetWindowRect(hwnd, &rc);
            int area = (rc.right - rc.left) * (rc.bottom - rc.top);
            if (!ctx->best || area > ctx->score) {
                ctx->best  = hwnd;
                ctx->score = area;
            }
        }
    }
    CloseHandle(hProc);
    return TRUE;
}

static void BringSourceAppToFront(const std::wstring& aumid) {
    FindWindowCtx ctx;
    ctx.aumid  = aumid;
    ctx.hint   = ExtractExeHint(aumid);
    EnumWindows(FindWindowByAppIdProc, reinterpret_cast<LPARAM>(&ctx));

    // Secondary pass if primary yielded only a hint match (score < 100)
    if (!ctx.best || ctx.score < 100) {
        FindWindowCtx ctx2;
        ctx2.aumid  = aumid;
        ctx2.hint   = ctx.hint;
        EnumWindows(FindBestWindowProc, reinterpret_cast<LPARAM>(&ctx2));
        if (ctx2.best) ctx.best = ctx2.best;
    }

    if (!ctx.best) return;

    if (IsIconic(ctx.best)) {
        ShowWindow(ctx.best, SW_RESTORE);
        AllowSetForegroundWindow(ASFW_ANY);
        DWORD ourTid = GetCurrentThreadId();
        HWND hwndFg  = GetForegroundWindow();
        DWORD fgTid  = GetWindowThreadProcessId(hwndFg, nullptr);
        if (ourTid != fgTid) AttachThreadInput(ourTid, fgTid, TRUE);
        BringWindowToTop(ctx.best);
        SetForegroundWindow(ctx.best);
        if (ourTid != fgTid) AttachThreadInput(ourTid, fgTid, FALSE);
    } else {
        ShowWindow(ctx.best, SW_MINIMIZE);
    }
}

// ---------- SC-FLY-1: flyout Win32 window ----------

// Returns true when apps should use dark mode.
// Resolves uxtheme.dll ordinal 132 (ShouldAppsUseDarkMode) — the same function
// Windows itself uses, more reliable than registry reads in injected contexts.
static bool IsSystemDarkMode() {
    using Fn = bool (WINAPI*)();
    static Fn fn = []() -> Fn {
        HMODULE ux = GetModuleHandleW(L"uxtheme.dll");
        if (!ux) ux = LoadLibraryW(L"uxtheme.dll");
        return ux ? reinterpret_cast<Fn>(GetProcAddress(ux, (LPCSTR)132)) : nullptr;
    }();
    return fn ? fn() : false;
}

// Width matches the widget (g_Settings.panelWidth); height is fixed.
// Layout: full-width album art square at top, title + artist stacked below.
static constexpr int kFlyoutHDIPs   = 380;
static constexpr int kFlyoutPadDIPs = 12;

static LRESULT CALLBACK FlyoutWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        DWORD corner = 2;  // DWMWCP_ROUND
        DwmSetWindowAttribute(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &corner, sizeof(corner));
        BOOL dark = IsSystemDarkMode() ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
        return 0;
    }
    case WM_SETTINGCHANGE:
        if (lp && lstrcmpiW(reinterpret_cast<LPCWSTR>(lp), L"ImmersiveColorSet") == 0) {
            BOOL dark = IsSystemDarkMode() ? TRUE : FALSE;
            DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;  // suppress flicker; WM_PAINT fills everything
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int W = rc.right, H = rc.bottom;

        bool darkMode = IsSystemDarkMode();
        // Dark: neutral gray matching widget's AcrylicBrush TintColor (no blue cast).
        // Light: GetSysColor(COLOR_3DFACE) is the OS panel/dialog surface — tracks theme.
        COLORREF clrBg     = darkMode ? RGB(28, 28, 28) : GetSysColor(COLOR_3DFACE);
        COLORREF clrTitle  = darkMode ? RGB(240, 240, 240) : GetSysColor(COLOR_WINDOWTEXT);
        COLORREF clrArtist = darkMode ? RGB(160, 160, 180) : GetSysColor(COLOR_GRAYTEXT);

        HBRUSH bgBrush = CreateSolidBrush(clrBg);
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);

        // Read title and artist under lock
        std::wstring title, artist;
        {
            std::lock_guard<std::mutex> lk(g_FlyoutContentMutex);
            title  = g_FlyoutTitleStr;
            artist = g_FlyoutArtistStr;
        }

        int logPx = GetDeviceCaps(hdc, LOGPIXELSY);
        int pad   = MulDiv(kFlyoutPadDIPs, logPx, 96);
        // Art square fills the full width (minus padding on each side).
        int artSz = W - 2 * pad;
        SetBkMode(hdc, TRANSPARENT);

        // Album art — full-width square at top, HALFTONE for quality scaling
        {
            std::lock_guard<std::mutex> lk(g_FlyoutArtMutex);
            if (g_FlyoutArtHBitmap) {
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, g_FlyoutArtHBitmap);
                SetStretchBltMode(hdc, HALFTONE);
                SetBrushOrgEx(hdc, 0, 0, nullptr);
                StretchBlt(hdc, pad, pad, artSz, artSz,
                           memDC, 0, 0, g_FlyoutArtBmpW, g_FlyoutArtBmpH, SRCCOPY);
                SelectObject(memDC, oldBmp);
                DeleteDC(memDC);
            }
        }

        // Text zone: below the art, split evenly between title and artist
        int textTop = pad + artSz + MulDiv(8, logPx, 96);
        int textH   = H - textTop - pad;

        // Title — Segoe UI SemiBold, 13pt stepping down to 9pt to fit available width;
        // DT_END_ELLIPSIS is the final fallback if the text is still too wide at 9pt.
        int titlePt  = 13;
        int availW   = W - 2 * pad;
        HFONT fTitle = nullptr;
        HFONT fPrev  = nullptr;
        for (;;) {
            if (fTitle) { SelectObject(hdc, fPrev); DeleteObject(fTitle); }
            fTitle = CreateFontW(
                -MulDiv(titlePt, logPx, 72), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            fPrev = (HFONT)SelectObject(hdc, fTitle);
            SIZE sz = {};
            if (!title.empty())
                GetTextExtentPoint32W(hdc, title.c_str(), (int)title.size(), &sz);
            if (sz.cx <= availW || titlePt <= 9) break;
            --titlePt;
        }
        SetTextColor(hdc, clrTitle);
        RECT rTitle = { pad, textTop, W - pad, textTop + textH / 2 };
        DrawTextW(hdc, title.c_str(), -1, &rTitle,
                  DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(hdc, fPrev);
        DeleteObject(fTitle);

        // Artist — Segoe UI Regular 11pt, muted
        HFONT fArtist = CreateFontW(
            -MulDiv(11, logPx, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        fPrev = (HFONT)SelectObject(hdc, fArtist);
        SetTextColor(hdc, clrArtist);
        RECT rArtist = { pad, textTop + textH / 2, W - pad, H - pad };
        DrawTextW(hdc, artist.c_str(), -1, &rArtist,
                  DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(hdc, fPrev);
        DeleteObject(fArtist);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_TIMER:
        if (wp == 1) {
            KillTimer(hwnd, 1);
            ShowWindow(hwnd, SW_HIDE);
        }
        return 0;
    case WM_FLYOUT_SHOW: {
        KillTimer(hwnd, 1);
        HWND tbar = g_hTaskbarWnd.load();
        if (!tbar) return 0;
        RECT tr; GetWindowRect(tbar, &tr);
        int dpi       = GetDpiForWindow(hwnd);
        int widgetWPx = MulDiv(g_Settings.panelWidth, dpi, 96);
        int flyoutH   = MulDiv(kFlyoutHDIPs,          dpi, 96);
        int taskbarW  = tr.right - tr.left;
        int x;
        switch (g_Settings.widgetPosition) {
        case WidgetPosition::Left:
            x = tr.left + MulDiv(g_Settings.offsetX, dpi, 96);
            break;
        case WidgetPosition::Center:
            x = tr.left + (taskbarW - widgetWPx) / 2
                        + MulDiv(g_Settings.offsetX, dpi, 96);
            break;
        default: // Right
            x = tr.right - MulDiv(g_FlyoutMarginDIPs.load(), dpi, 96) - widgetWPx;
            break;
        }
        int y = tr.top - flyoutH - MulDiv(4, dpi, 96);
        SetWindowPos(hwnd, HWND_TOPMOST, x, y, widgetWPx, flyoutH,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }
    case WM_FLYOUT_HIDE_DELAYED:
        SetTimer(hwnd, 1, 300, nullptr);
        return 0;
    case WM_FLYOUT_HIDE_NOW:
        KillTimer(hwnd, 1);
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    case WM_FLYOUT_UPDATE:
        if (IsWindowVisible(hwnd)) InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    case WM_FLYOUT_SETTINGS:
        SetLayeredWindowAttributes(hwnd, 0,
            g_Settings.flyoutTransparent ? 235 : 255, LWA_ALPHA);
        return 0;
    case WM_FLYOUT_QUIT:
        KillTimer(hwnd, 1);
        DestroyWindow(hwnd);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static DWORD WINAPI FlyoutThreadProc(LPVOID readyEvent) {
    static const wchar_t kClass[] = L"NativeTaskbarMediaFlyout";
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = FlyoutWndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);

    g_FlyoutHwnd.store(CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_LAYERED,
        kClass, nullptr, WS_POPUP,
        0, 0, 300, 380,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr));

    if (HWND fly = g_FlyoutHwnd.load()) {
        SetLayeredWindowAttributes(fly, 0,
            g_Settings.flyoutTransparent ? 235 : 255, LWA_ALPHA);
    }

    SetEvent((HANDLE)readyEvent);  // signal that HWND is ready (or creation failed)

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    g_FlyoutHwnd.store(nullptr);
    UnregisterClassW(kClass, GetModuleHandleW(nullptr));
    return 0;
}

// ---------- Widget construction ----------
static Grid BuildWidget() {
    Grid root;
    root.Name(kWidgetRootName);
    root.Width((double)g_Settings.panelWidth);
    root.HorizontalAlignment(
        g_Settings.widgetPosition == WidgetPosition::Left   ? HorizontalAlignment::Left   :
        g_Settings.widgetPosition == WidgetPosition::Center ? HorizontalAlignment::Center :
                                                              HorizontalAlignment::Right);
    root.VerticalAlignment(VerticalAlignment::Stretch);
    // Background (Acrylic) is applied on first call to ApplyStateToWidget().
    root.CornerRadius(CornerRadiusHelper::FromUniformRadius(8.0));
    Canvas::SetZIndex(root, 2);
    // Span all columns so right-alignment is relative to full taskbar width.
    Grid::SetColumnSpan(root, 9999);
    Grid::SetRowSpan(root, 9999);
    // Margin is set dynamically via UpdateWidgetMargin() based on the position mode.

    Grid layout;
    layout.HorizontalAlignment(HorizontalAlignment::Stretch);
    layout.VerticalAlignment(VerticalAlignment::Center);
    layout.Margin(ThicknessHelper::FromLengths(8.0, 0.0, 8.0, 0.0));

    auto cols = layout.ColumnDefinitions();
    for (int i = 0; i < 6; ++i) {
        ColumnDefinition cd;
        cd.Width(i == 2
            ? GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star)
            : GridLengthHelper::Auto());
        cols.Append(cd);
    }

    // Album art — square thumbnail at the left edge of the widget.
    // Hidden until art is loaded; size matches panel height for a flush square.
    Image albumArt;
    albumArt.Name(kAlbumArtName);
    albumArt.Width((double)g_Settings.panelHeight);
    albumArt.Height((double)g_Settings.panelHeight);
    albumArt.Stretch(Stretch::UniformToFill);          // crop to square; no letterbox
    albumArt.VerticalAlignment(VerticalAlignment::Stretch);
    albumArt.Margin(ThicknessHelper::FromLengths(0, 0, 6, 0));  // 6 px gap before text
    albumArt.Visibility(Visibility::Collapsed);        // revealed once bitmap is loaded
    Grid::SetColumn(albumArt, 0);
    layout.Children().Append(albumArt);                // first child (inserted before session chip below)

    // Session count chip — overlaid at the top-right corner of the widget root
    TextBlock sessionCount;
    sessionCount.Name(kSessionCountName);
    sessionCount.Text(L"");
    sessionCount.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
    sessionCount.FontSize(std::max(8.0, (double)g_Settings.fontSize - 2.0));
    sessionCount.HorizontalAlignment(HorizontalAlignment::Right);
    sessionCount.VerticalAlignment(VerticalAlignment::Top);
    sessionCount.Margin(ThicknessHelper::FromLengths(0, 3, 6, 0));
    sessionCount.Visibility(Visibility::Collapsed);
    AutomationProperties::SetName(sessionCount, L"Cycle media session");
    sessionCount.Tapped(TappedEventHandler(
        [](IInspectable const&, TappedRoutedEventArgs const& e) {
            std::lock_guard<std::mutex> g(g_MediaMutex);
            if (g_MediaStateCount > 1) {
                g_ActiveSessionIndex = (g_ActiveSessionIndex + 1) % g_MediaStateCount;
            }
            e.Handled(true);
            RefreshWidgetUI();
        }));

    // Title / artist column
    StackPanel textCol;
    textCol.Orientation(Orientation::Vertical);
    textCol.VerticalAlignment(VerticalAlignment::Center);

    // Title clip canvas — clips the scrolling content to the star column width.
    Canvas titleCanvas;
    titleCanvas.Name(kTitleCanvasName);
    titleCanvas.HorizontalAlignment(HorizontalAlignment::Stretch);
    titleCanvas.VerticalAlignment(VerticalAlignment::Center);
    titleCanvas.Height((double)g_Settings.fontSize * 1.6);

    RectangleGeometry clipRect;
    clipRect.Rect(RectHelper::FromCoordinatesAndDimensions(
        0, 0, 0, (float)(g_Settings.fontSize * 1.6)));
    titleCanvas.Clip(clipRect);

    // Horizontal scroller — holds two copies of the title side by side.
    // Animating its TranslateTransform creates a seamless ticker: as copy 1 exits
    // left, copy 2 enters from the right at the same speed with no blank gap.
    StackPanel scroller;
    scroller.Name(kTitleScrollerName);
    scroller.Orientation(Orientation::Horizontal);
    scroller.VerticalAlignment(VerticalAlignment::Center);
    TranslateTransform scrollTranslate;
    scroller.RenderTransform(scrollTranslate);

    TextBlock title;
    title.Name(kTitleName);
    title.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
    title.FontSize((double)g_Settings.fontSize);
    title.TextTrimming(TextTrimming::None);
    title.TextWrapping(TextWrapping::NoWrap);
    title.MaxLines(1);
    title.VerticalAlignment(VerticalAlignment::Center);

    // Fixed gap between end of copy 1 and start of copy 2
    Border titleGap;
    titleGap.Width(48.0);

    TextBlock title2;
    title2.Name(kTitleName2);
    title2.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
    title2.FontSize((double)g_Settings.fontSize);
    title2.TextTrimming(TextTrimming::None);
    title2.TextWrapping(TextWrapping::NoWrap);
    title2.MaxLines(1);
    title2.VerticalAlignment(VerticalAlignment::Center);

    scroller.Children().Append(title);
    scroller.Children().Append(titleGap);
    scroller.Children().Append(title2);
    titleCanvas.Children().Append(scroller);

    // SizeChanged on the Canvas keeps the clip rect width current and re-evaluates
    // marquee — this fires after the Canvas gets its real width from the star column,
    // which may be after the TextBlock's first SizeChanged fires with canvasW==0.
    auto weakCanvas = make_weak(titleCanvas);
    auto weakTitle  = make_weak(title);
    titleCanvas.SizeChanged(SizeChangedEventHandler(
        [weakCanvas, weakTitle](IInspectable const& sender, SizeChangedEventArgs const& e) {
            auto canvas = sender.as<Canvas>();
            if (auto clip = canvas.Clip().try_as<RectangleGeometry>()) {
                auto r = clip.Rect();
                clip.Rect(RectHelper::FromCoordinatesAndDimensions(
                    0, 0, (float)e.NewSize().Width, r.Height));
            }
            if (!g_Settings.marqueeScroll) return;
            auto cv = weakCanvas.get();
            auto tb = weakTitle.get();
            if (cv && tb) StartMarqueeIfNeeded(cv, tb);
        }));

    // SizeChanged on the TextBlock re-evaluates marquee when text changes width.
    g_TitleSizeChangedToken = title.SizeChanged(SizeChangedEventHandler(
        [weakCanvas, weakTitle](IInspectable const&, SizeChangedEventArgs const&) {
            auto cv = weakCanvas.get();
            auto tb = weakTitle.get();
            if (!cv || !tb) return;
            if (g_Settings.marqueeScroll)
                StartMarqueeIfNeeded(cv, tb);
        }));

    textCol.Children().Append(titleCanvas);

    TextBlock artist;
    artist.Name(kArtistName);
    artist.Foreground(MakeBrush(0xB3, 0xFF, 0xFF, 0xFF));
    artist.FontSize((double)g_Settings.fontSize);
    artist.TextTrimming(TextTrimming::CharacterEllipsis);
    artist.TextWrapping(TextWrapping::NoWrap);
    artist.MaxLines(1);

    textCol.Children().Append(artist);

    TextBlock timestamp;
    timestamp.Name(kTimestampName);
    timestamp.FontSize(std::max(8.0, (double)g_Settings.fontSize - 2.0));
    timestamp.Foreground(MakeBrush(0xB3, 0xFF, 0xFF, 0xFF));
    timestamp.TextTrimming(TextTrimming::CharacterEllipsis);
    timestamp.TextWrapping(TextWrapping::NoWrap);
    timestamp.MaxLines(1);
    timestamp.Visibility(Visibility::Collapsed);
    textCol.Children().Append(timestamp);

    Grid::SetColumn(textCol, 2);
    layout.Children().Append(textCol);

    // 2c: Skip Backward — previous track/chapter; shown only when session enables it
    Button skipBack;
    skipBack.Name(kSkipBackName);
    skipBack.Content(box_value(hstring{L""})); // MDL2 Previous
    skipBack.FontFamily(FontFamily(L"Segoe MDL2 Assets"));
    skipBack.Background(MakeBrush(0x00, 0, 0, 0));
    skipBack.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
    skipBack.BorderThickness(ThicknessHelper::FromUniformLength(0));
    skipBack.Padding(ThicknessHelper::FromLengths(6, 2, 6, 2));
    skipBack.Margin(ThicknessHelper::FromLengths(8, 0, 2, 0));
    skipBack.VerticalAlignment(VerticalAlignment::Center);
    AutomationProperties::SetName(skipBack, L"Skip backward");
    skipBack.Click(RoutedEventHandler(
        [](IInspectable const&, RoutedEventArgs const&) {
            GlobalSystemMediaTransportControlsSession s{ nullptr };
            {
                std::lock_guard<std::mutex> g(g_MediaMutex);
                if (g_ActiveSessionIndex >= 0 && g_ActiveSessionIndex < g_MediaStateCount) {
                    s = g_MediaStates[g_ActiveSessionIndex].session;
                }
            }
            if (s) {
                try { s.TrySkipPreviousAsync(); } WH_CATCH(L"SkipBack/SkipPrevious")
            }
        }));
    Grid::SetColumn(skipBack, 3);
    layout.Children().Append(skipBack);

    // Play/Pause
    Button playPause;
    playPause.Name(kPlayPauseName);
    playPause.Content(box_value(hstring{L""})); // MDL2 Play
    playPause.FontFamily(FontFamily(L"Segoe MDL2 Assets"));
    playPause.Background(MakeBrush(0x00, 0, 0, 0));
    playPause.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
    playPause.BorderThickness(ThicknessHelper::FromUniformLength(0));
    playPause.Padding(ThicknessHelper::FromLengths(4, 2, 4, 2));
    playPause.Margin(ThicknessHelper::FromLengths(2, 0, 2, 0));
    playPause.Width(30.0); // fixed width prevents layout shift when toggling play/pause
    playPause.VerticalAlignment(VerticalAlignment::Center);
    AutomationProperties::SetName(playPause, L"Play or pause");
    playPause.Click(RoutedEventHandler(
        [](IInspectable const&, RoutedEventArgs const&) {
            GlobalSystemMediaTransportControlsSession s{ nullptr };
            {
                std::lock_guard<std::mutex> g(g_MediaMutex);
                if (g_ActiveSessionIndex >= 0 && g_ActiveSessionIndex < g_MediaStateCount) {
                    s = g_MediaStates[g_ActiveSessionIndex].session;
                }
            }
            if (s) {
                try { s.TryTogglePlayPauseAsync(); } WH_CATCH(L"PlayPause/TogglePlayPause")
            }
        }));
    Grid::SetColumn(playPause, 4);
    layout.Children().Append(playPause);

    // 2c: Skip Forward — next track/chapter; shown only when session enables it
    Button skipFwd;
    skipFwd.Name(kSkipFwdName);
    skipFwd.Content(box_value(hstring{L""})); // MDL2 Next
    skipFwd.FontFamily(FontFamily(L"Segoe MDL2 Assets"));
    skipFwd.Background(MakeBrush(0x00, 0, 0, 0));
    skipFwd.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
    skipFwd.BorderThickness(ThicknessHelper::FromUniformLength(0));
    skipFwd.Padding(ThicknessHelper::FromLengths(6, 2, 6, 2));
    skipFwd.Margin(ThicknessHelper::FromLengths(2, 0, 0, 0));
    skipFwd.VerticalAlignment(VerticalAlignment::Center);
    AutomationProperties::SetName(skipFwd, L"Skip forward");
    skipFwd.Click(RoutedEventHandler(
        [](IInspectable const&, RoutedEventArgs const&) {
            GlobalSystemMediaTransportControlsSession s{ nullptr };
            {
                std::lock_guard<std::mutex> g(g_MediaMutex);
                if (g_ActiveSessionIndex >= 0 && g_ActiveSessionIndex < g_MediaStateCount) {
                    s = g_MediaStates[g_ActiveSessionIndex].session;
                }
            }
            if (s) {
                try { s.TrySkipNextAsync(); } WH_CATCH(L"SkipFwd/SkipForward")
            }
        }));
    Grid::SetColumn(skipFwd, 5);
    layout.Children().Append(skipFwd);

    root.Children().Append(layout);
    root.Children().Append(sessionCount);

    // SC-KV-4: progress bar — two stacked Rectangles (track + fill) so we own
    // the colors completely; ProgressBar's internal template uses accent color.
    Grid progressTrack;
    progressTrack.Name(kProgressTrackName);
    progressTrack.VerticalAlignment(VerticalAlignment::Bottom);
    progressTrack.Height(3.0);
    progressTrack.Background(MakeBrush(0x33, 0xFF, 0xFF, 0xFF));  // subtle rail
    progressTrack.Visibility(Visibility::Collapsed);

    Shapes::Rectangle progressFill;
    progressFill.Name(kProgressFillName);
    progressFill.HorizontalAlignment(HorizontalAlignment::Left);
    progressFill.Fill(MakeBrush(0xCC, 0xFF, 0xFF, 0xFF));         // bright fill
    progressFill.Width(0.0);

    progressTrack.Children().Append(progressFill);
    root.Children().Append(progressTrack);

    // SC-M-2: double-tap the widget to raise the source app to the foreground.
    // EnumWindows is synchronous but fast enough for a user gesture on the UI thread.
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

    // SC-M-3: middle-click stops the active media session (TryStopAsync — closest public API to "close session").
    root.PointerPressed(PointerEventHandler(
        [](IInspectable const&, PointerRoutedEventArgs const& e) {
            using Windows::UI::Input::PointerUpdateKind;
            if (e.GetCurrentPoint(nullptr).Properties().PointerUpdateKind()
                    != PointerUpdateKind::MiddleButtonPressed) return;
            e.Handled(true);
            GlobalSystemMediaTransportControlsSession session{ nullptr };
            {
                std::lock_guard<std::mutex> g(g_MediaMutex);
                if (g_ActiveSessionIndex >= 0 && g_ActiveSessionIndex < g_MediaStateCount)
                    session = g_MediaStates[g_ActiveSessionIndex].session;
            }
            if (session) {
                [](GlobalSystemMediaTransportControlsSession s) -> winrt::fire_and_forget {
                    try { co_await s.TryStopAsync(); } catch (...) {}
                }(session);
            }
        }));

    // SC-FLY-1: show flyout on hover, hide 300ms after cursor leaves.
    // Flyout is a Win32 WS_POPUP HWND on its own thread; cross-thread via PostMessageW.
    root.PointerEntered(PointerEventHandler(
        [](IInspectable const&, PointerRoutedEventArgs const&) {
            std::wstring title, artist;
            {
                std::lock_guard<std::mutex> mk(g_MediaMutex);
                if (g_ActiveSessionIndex >= 0 && g_ActiveSessionIndex < g_MediaStateCount) {
                    title  = g_MediaStates[g_ActiveSessionIndex].title;
                    artist = g_MediaStates[g_ActiveSessionIndex].artist;
                }
            }
            {
                std::lock_guard<std::mutex> lk(g_FlyoutContentMutex);
                g_FlyoutTitleStr  = std::move(title);
                g_FlyoutArtistStr = std::move(artist);
            }
            if (HWND fly = g_FlyoutHwnd.load()) PostMessageW(fly, WM_FLYOUT_SHOW, 0, 0);
        }));
    root.PointerExited(PointerEventHandler(
        [](IInspectable const&, PointerRoutedEventArgs const&) {
            if (HWND fly = g_FlyoutHwnd.load()) PostMessageW(fly, WM_FLYOUT_HIDE_DELAYED, 0, 0);
        }));

    return root;
}

template <typename T>
static T FindByName(FrameworkElement parent, std::wstring_view name) {
    if (!parent) return nullptr;
    auto fe = parent.try_as<FrameworkElement>();
    if (fe && std::wstring(fe.Name()) == name) {
        if (auto t = fe.try_as<T>()) return t;
    }
    int count = VisualTreeHelper::GetChildrenCount(parent);
    for (int i = 0; i < count; ++i) {
        auto child = VisualTreeHelper::GetChild(parent, i);
        if (!child) continue;
        auto cfe = child.try_as<FrameworkElement>();
        if (!cfe) continue;
        if (auto found = FindByName<T>(cfe, name)) return found;
    }
    return nullptr;
}

static std::wstring FormatMs(int64_t ms, bool forceHours = false) {
    int64_t secs = ms / 1000;
    int h = (int)(secs / 3600);
    int m = (int)((secs % 3600) / 60);
    int s = (int)(secs % 60);
    wchar_t buf[16];
    if (h > 0 || forceHours) swprintf(buf, 16, L"%d:%02d:%02d", h, m, s);
    else                     swprintf(buf, 16, L"%d:%02d", m, s);
    return std::wstring(buf);
}

// ---------- Marquee scroll helpers ----------

// Convert seconds to WinRT TimeSpan (100-nanosecond intervals).
static TimeSpan MakeTimeSpan(double seconds) {
    return TimeSpan{ static_cast<int64_t>(seconds * 10'000'000) };
}

static void StopMarquee() {
    if (g_MarqueeStoryboard) {
        try { g_MarqueeStoryboard.Stop(); } catch (...) {}
        g_MarqueeStoryboard = nullptr;
    }
}

// SC-HT-4: stop the interpolation timer (safe to call when already null).
static void StopProgressTimer() {
    if (g_ProgressTimer) {
        try { g_ProgressTimer.Stop(); } catch (...) {}
        g_ProgressTimer = nullptr;
    }
}

// SC-HT-4: start a 500ms DispatcherTimer that advances the displayed
// progress fill by elapsed wall-time between SMTC updates.
static void StartProgressTimer(Grid widget) {
    StopProgressTimer();
    auto weakWidget = make_weak(widget);
    DispatcherTimer timer;
    timer.Interval(MakeTimeSpan(0.5));
    timer.Tick([weakWidget](IInspectable const&, IInspectable const&) {
        // Read session state under mutex — brief critical section.
        int64_t posMs = 0, durMs = 0;
        bool playing = false;
        {
            std::lock_guard<std::mutex> lk(g_MediaMutex);
            int idx = g_ActiveSessionIndex;
            if (idx >= 0 && idx < g_MediaStateCount) {
                posMs   = g_MediaStates[idx].positionMs;
                durMs   = g_MediaStates[idx].durationMs;
                playing = g_MediaStates[idx].isPlaying;
            }
        }
        if (!playing || durMs <= 0) {
            StopProgressTimer();
            return;
        }
        ULONGLONG now     = GetTickCount64();
        ULONGLONG elapsed = now - g_ProgressLastTickMs;
        g_ProgressLastTickMs = now;
        int64_t displayPos = std::min(posMs + (int64_t)elapsed, durMs);

        auto w = weakWidget.get();
        if (!w) { StopProgressTimer(); return; }

        // Update fill width
        if (auto track = FindByName<Grid>(w, kProgressTrackName)) {
            if (auto fill = FindByName<Shapes::Rectangle>(track, kProgressFillName)) {
                double ratio = std::clamp(displayPos / (double)durMs, 0.0, 1.0);
                fill.Width(ratio * track.ActualWidth());
            }
        }
        // Update timestamp
        if (auto tsTb = FindByName<TextBlock>(w, kTimestampName)) {
            if (tsTb.Visibility() == Visibility::Visible) {
                bool hasHours = durMs >= 3'600'000LL;
                tsTb.Text(FormatMs(displayPos, hasHours) + L" / " + FormatMs(durMs));
            }
        }
    });
    g_ProgressTimer = timer;
    timer.Start();
}

// SC-SP-4: fade the widget in (visible=true) or out (visible=false) over 0.2s.
// Must only be called from the XAML dispatcher thread.
static void SetWidgetVisible(UIElement el, bool visible) {
    bool currentlyVisible = (el.Visibility() == Visibility::Visible);
    // No-op when already in the target stable state with no transition in flight.
    if ( visible && currentlyVisible && !g_WidgetFadeStoryboard) return;
    if (!visible && !currentlyVisible)                            return;
    // Don't restart a storyboard already animating toward the same target — that
    // would snap opacity to 0 mid-animation and cause a visible flash.
    if (g_WidgetFadeStoryboard && g_WidgetFadeTargetVisible == visible) return;

    if (g_WidgetFadeStoryboard) {
        try { g_WidgetFadeStoryboard.Stop(); } catch (...) {}
        g_WidgetFadeStoryboard = nullptr;
    }

    DoubleAnimation anim;
    anim.From(visible ? 0.0 : 1.0);
    anim.To  (visible ? 1.0 : 0.0);
    anim.Duration(DurationHelper::FromTimeSpan(MakeTimeSpan(0.2)));
    anim.EnableDependentAnimation(true);

    Storyboard sb;
    Storyboard::SetTarget(anim, el);
    Storyboard::SetTargetProperty(anim, L"Opacity");
    sb.Children().Append(anim);

    if (visible) {
        el.Opacity(0.0);
        el.Visibility(Visibility::Visible);
    }

    if (!visible) {
        auto weakEl = make_weak(el);
        sb.Completed([weakEl](IInspectable const&, IInspectable const&) {
            if (auto e = weakEl.get()) e.Visibility(Visibility::Collapsed);
        });
    }

    g_WidgetFadeTargetVisible = visible;
    g_WidgetFadeStoryboard = sb;
    sb.Begin();
}

static void StartMarqueeIfNeeded(Canvas titleCanvas, TextBlock titleTb) {
    StopMarquee();

    double canvasW = titleCanvas.ActualWidth();
    double textW   = titleTb.ActualWidth();  // natural width of copy 1

    auto scroller = FindByName<StackPanel>(titleCanvas, kTitleScrollerName);

    if (canvasW <= 0 || textW <= canvasW) {
        // Text fits — reset scroller position and leave static.
        if (scroller)
            if (auto tt = scroller.RenderTransform().try_as<TranslateTransform>())
                tt.X(0.0);
        return;
    }
    if (!scroller) return;

    // Cycle distance = title width + gap. At the end of one cycle the scroller
    // has moved exactly enough that copy 2 sits where copy 1 started — the loop
    // reset is invisible and copy 2 enters the clip before copy 1 fully exits.
    constexpr double kGap = 48.0;
    double cycleDist = textW + kGap;
    double cycleSecs = cycleDist / 50.0;  // 50 px/s constant speed
    double startPause = 2.0;
    double totalSecs  = startPause + cycleSecs;

    TranslateTransform tt = scroller.RenderTransform().as<TranslateTransform>();
    tt.X(0.0);

    DoubleAnimationUsingKeyFrames anim;

    DiscreteDoubleKeyFrame kfHold;
    kfHold.KeyTime(KeyTimeHelper::FromTimeSpan(MakeTimeSpan(startPause)));
    kfHold.Value(0.0);
    anim.KeyFrames().Append(kfHold);

    LinearDoubleKeyFrame kfScroll;
    kfScroll.KeyTime(KeyTimeHelper::FromTimeSpan(MakeTimeSpan(totalSecs)));
    kfScroll.Value(-cycleDist);
    anim.KeyFrames().Append(kfScroll);

    anim.Duration(DurationHelper::FromTimeSpan(MakeTimeSpan(totalSecs)));
    anim.RepeatBehavior(RepeatBehaviorHelper::Forever());

    Storyboard sb;
    Storyboard::SetTarget(anim, scroller);
    Storyboard::SetTargetProperty(anim,
        L"(UIElement.RenderTransform).(TranslateTransform.X)");
    sb.Children().Append(anim);

    g_MarqueeStoryboard = sb;
    sb.Begin();
}

static void ApplyStateToWidget(Grid widget) {
    if (!widget) return;

    int count = 0;
    int activeIdx = 0;
    bool hasMedia = false;
    std::wstring title, artist;
    bool isPlaying = false;
    double playbackRate = 1.0;
    bool canSkipForward = false, canSkipBackward = false;
    uint32_t thumbnailVersion = 0;
    IRandomAccessStreamReference thumbnailRef{ nullptr };
    int64_t positionMs = 0, durationMs = 0;
    bool isAudiobook = false;
    SessionSource source = SessionSource::Unknown;
    {
        std::lock_guard<std::mutex> g(g_MediaMutex);
        count = g_MediaStateCount;
        activeIdx = g_ActiveSessionIndex;
        if (activeIdx >= 0 && activeIdx < count) {
            auto& m = g_MediaStates[activeIdx];
            title            = m.title;
            artist           = m.artist;
            isPlaying        = m.isPlaying;
            playbackRate     = m.playbackRate;
            canSkipForward   = m.canSkipForward;
            canSkipBackward  = m.canSkipBackward;
            thumbnailVersion = m.thumbnailVersion;
            thumbnailRef     = m.thumbnailRef;
            positionMs       = m.positionMs;
            durationMs       = m.durationMs;
            isAudiobook      = m.isAudiobook;
            source           = m.source;
            hasMedia = true;
        }
    }

    // 2b: Build artist display string; append rate suffix when speed != 1.0×
    std::wstring artistDisplay = artist;
    // Rate == 0.0 means paused (SMTC reports 0 for PlaybackRate when not playing).
    if (playbackRate > 0.01 && std::fabs(playbackRate - 1.0) > 0.01) {
        wchar_t rateBuf[16];
        swprintf(rateBuf, 16, L"%.4g\u00D7", playbackRate); // e.g. "1.5×"
        artistDisplay += artistDisplay.empty() ? rateBuf
                                               : (std::wstring(L" \u00B7 ") + rateBuf);
    }

    auto titleTb    = FindByName<TextBlock>(widget, kTitleName);
    auto titleTb2   = FindByName<TextBlock>(widget, kTitleName2);
    auto artistTb   = FindByName<TextBlock>(widget, kArtistName);
    auto playBtn    = FindByName<Button>(widget, kPlayPauseName);
    auto sessTb     = FindByName<TextBlock>(widget, kSessionCountName);
    auto skipFwdBtn = FindByName<Button>(widget, kSkipFwdName);
    auto skipBackBtn= FindByName<Button>(widget, kSkipBackName);
    auto artEl      = FindByName<Image>(widget, kAlbumArtName);

    // SC-GR-2: build the new artist display string for change detection.
    hstring newArtist{ hasMedia ? artistDisplay : std::wstring{} };

    bool handledByFade = false;
    if (titleTb) {
        hstring newTitle{ hasMedia ? title : std::wstring{} };
        bool titleChanged  = (titleTb.Text()  != newTitle);
        bool artistChanged = (artistTb && artistTb.Text() != newArtist);
        if (titleChanged || artistChanged) {
            // Track changed — crossfade: fade out, swap text, fade back in.
            // Only reset scroll on a track change.
            if (titleChanged) {
                StopMarquee();
                if (auto scroller = FindByName<StackPanel>(widget, kTitleScrollerName))
                    if (auto tt = scroller.RenderTransform().try_as<TranslateTransform>())
                        tt.X(0.0);
            }

            // Stop any running text fade storyboard.
            if (g_TextFadeStoryboard) {
                try { g_TextFadeStoryboard.Stop(); } catch (...) {}
                g_TextFadeStoryboard = nullptr;
            }

            // Build the scroller panel element (parent of title1+title2) for fade target.
            auto titleScroller = FindByName<StackPanel>(widget, kTitleScrollerName);

            // Fade-out storyboard (0.15 s).
            Storyboard fadeOut;
            auto addFade = [&](UIElement target, double from, double to) {
                DoubleAnimation a;
                a.From(from); a.To(to);
                a.Duration(DurationHelper::FromTimeSpan(MakeTimeSpan(0.15)));
                a.EnableDependentAnimation(true);
                Storyboard::SetTarget(a, target);
                Storyboard::SetTargetProperty(a, L"Opacity");
                fadeOut.Children().Append(a);
            };
            if (titleScroller) addFade(titleScroller, 1.0, 0.0);
            if (artistTb)      addFade(artistTb,      1.0, 0.0);

            // Capture everything the Completed lambda needs as weak refs / values.
            auto weakTitle1   = make_weak(titleTb);
            auto weakTitle2   = make_weak(titleTb2);
            auto weakArtistTb = make_weak(artistTb);
            auto weakScroller = titleScroller ? make_weak(titleScroller)
                                             : weak_ref<StackPanel>{ nullptr };
            hstring capturedTitle  = newTitle;
            hstring capturedArtist = newArtist;

            fadeOut.Completed([weakTitle1, weakTitle2, weakArtistTb, weakScroller,
                               capturedTitle, capturedArtist]
                              (IInspectable const&, IInspectable const&) {
                // Swap text while invisible.
                if (auto t1 = weakTitle1.get())   t1.Text(capturedTitle);
                if (auto t2 = weakTitle2.get())   t2.Text(capturedTitle);
                if (auto at = weakArtistTb.get()) at.Text(capturedArtist);

                // Fade-in storyboard (0.15 s).
                Storyboard fadeIn;
                auto addIn = [&](UIElement target) {
                    DoubleAnimation a;
                    a.From(0.0); a.To(1.0);
                    a.Duration(DurationHelper::FromTimeSpan(MakeTimeSpan(0.15)));
                    a.EnableDependentAnimation(true);
                    Storyboard::SetTarget(a, target);
                    Storyboard::SetTargetProperty(a, L"Opacity");
                    fadeIn.Children().Append(a);
                };
                if (auto sc = weakScroller.get()) addIn(sc);
                if (auto at = weakArtistTb.get()) addIn(at);
                fadeIn.Begin();
            });

            g_TextFadeStoryboard = fadeOut;
            fadeOut.Begin();
            handledByFade = true;
        }
    }
    if (!handledByFade) {
        // State-only update (play/pause, color, etc.) — write text directly, no animation.
        if (artistTb && artistTb.Text() != newArtist) artistTb.Text(newArtist);
    }
    if (playBtn)  playBtn.Content(box_value(hstring{isPlaying ? L"\uE769" : L"\uE768"})); // MDL2 Pause / Play

    // Audiobook mode: relabel artist field for screen readers (author vs. artist).
    if (artistTb)
        AutomationProperties::SetName(artistTb, isAudiobook ? L"Author" : L"Artist");

    // SC-UI-2: adaptive foreground follows Windows light/dark theme.
    // Light taskbar → near-black text; dark taskbar → white text.
    bool lightBg = g_Settings.adaptiveTextColor && IsSystemLightTheme();
    uint8_t fgHi = lightBg ? 0x1A : 0xFF;
    if (titleTb)   titleTb.Foreground(MakeBrush(0xFF, fgHi, fgHi, fgHi));
    if (titleTb2)  titleTb2.Foreground(MakeBrush(0xFF, fgHi, fgHi, fgHi));
    if (artistTb)  artistTb.Foreground(MakeBrush(0xB3, fgHi, fgHi, fgHi));
    if (sessTb)    sessTb.Foreground(MakeBrush(0xFF, fgHi, fgHi, fgHi));
    auto btnFg = MakeBrush(0xFF, fgHi, fgHi, fgHi);
    if (playBtn)    playBtn.Foreground(btnFg);
    if (skipFwdBtn) skipFwdBtn.Foreground(btnFg);
    if (skipBackBtn)skipBackBtn.Foreground(btnFg);

    if (sessTb) {
        if (count > 1) {
            sessTb.Text(std::to_wstring(count));
            sessTb.Visibility(Visibility::Visible);
        } else {
            sessTb.Text(L"");
            sessTb.Visibility(Visibility::Collapsed);
        }
    }

    // 2c: Skip buttons always visible; dim when the source doesn't support them.
    if (skipBackBtn) skipBackBtn.Opacity(canSkipBackward ? 1.0 : 0.35);
    if (skipFwdBtn)  skipFwdBtn.Opacity(canSkipForward  ? 1.0 : 0.35);

    // Audiobook mode: relabel skip buttons for screen readers (chapter vs. track).
    if (skipBackBtn)
        AutomationProperties::SetName(skipBackBtn,
            isAudiobook ? L"Previous chapter" : L"Skip backward");
    if (skipFwdBtn)
        AutomationProperties::SetName(skipFwdBtn,
            isAudiobook ? L"Next chapter" : L"Skip forward");

    // Album art: clear on no-media, else kick off async load
    if (artEl) {
        if (!hasMedia || !thumbnailRef) {
            // No session / Libby / app with no thumbnail — collapse and clear.
            artEl.Source(nullptr);
            artEl.Visibility(Visibility::Collapsed);
            // SC-FLY-1: clear flyout art bitmap when no thumbnail is available.
            {
                std::lock_guard<std::mutex> lk(g_FlyoutArtMutex);
                if (g_FlyoutArtHBitmap) {
                    DeleteObject(g_FlyoutArtHBitmap);
                    g_FlyoutArtHBitmap = nullptr;
                    g_FlyoutArtBmpW = g_FlyoutArtBmpH = 0;
                }
            }
            if (HWND fly = g_FlyoutHwnd.load()) PostMessageW(fly, WM_FLYOUT_UPDATE, 0, 0);
        } else {
            // Art available — open stream async and decode into BitmapImage.
            // This coroutine always starts on the UI dispatcher thread (called
            // from ApplyStateToWidget which runs inside RunAsync). C++/WinRT's
            // apartment_aware_awaiter resumes both co_awaits on the same UI
            // thread STA, so BitmapImage (which requires the UI thread) is
            // always created in the correct apartment — no extra RunAsync needed.
            auto weakArt  = make_weak(artEl);
            auto ref      = thumbnailRef;
            auto version  = thumbnailVersion;
            [](weak_ref<Image> weakEl,
               IRandomAccessStreamReference ref,
               uint32_t version) -> winrt::fire_and_forget {
                g_AsyncTasks++;
                struct Guard { ~Guard() { g_AsyncTasks--; } } g;
                (void)version;  // reserved for future stale-load detection

                try {
                    auto stream = co_await ref.OpenReadAsync();
                    if (g_Unloading.load()) co_return;

                    BitmapImage bitmap;
                    co_await bitmap.SetSourceAsync(stream);
                    if (g_Unloading.load()) co_return;

                    auto el = weakEl.get();
                    if (!el) co_return;
                    el.Source(bitmap);
                    el.Visibility(Visibility::Visible);

                    // SC-FLY-1: decode art to HBITMAP for the flyout panel.
                    // Uses GetPixelDataAsync/DetachPixelData — avoids IMemoryBufferByteAccess
                    // COM QI which fails in the taskbar's WinRT context.
                    try {
                        auto streamF = co_await ref.OpenReadAsync();
                        if (g_Unloading.load()) co_return;
                        auto decoder = co_await BitmapDecoder::CreateAsync(streamF);
                        uint32_t bmpW = decoder.PixelWidth();
                        uint32_t bmpH = decoder.PixelHeight();
                        if (bmpW && bmpH) {
                            auto pixelData = co_await decoder.GetPixelDataAsync(
                                BitmapPixelFormat::Bgra8,
                                BitmapAlphaMode::Ignore,
                                BitmapTransform{},
                                ExifOrientationMode::IgnoreExifOrientation,
                                ColorManagementMode::DoNotColorManage);
                            if (g_Unloading.load()) co_return;
                            auto bytes = pixelData.DetachPixelData();
                            uint32_t stride = bmpW * 4;
                            if (bytes.size() >= stride * bmpH) {
                                BITMAPINFOHEADER bmi = {};
                                bmi.biSize        = sizeof(bmi);
                                bmi.biWidth       = (LONG)bmpW;
                                bmi.biHeight      = -(LONG)bmpH;
                                bmi.biPlanes      = 1;
                                bmi.biBitCount    = 32;
                                bmi.biCompression = BI_RGB;
                                HDC screenDC = GetDC(nullptr);
                                void* bits = nullptr;
                                HBITMAP hbm = CreateDIBSection(screenDC,
                                    reinterpret_cast<BITMAPINFO*>(&bmi),
                                    DIB_RGB_COLORS, &bits, nullptr, 0);
                                ReleaseDC(nullptr, screenDC);
                                if (hbm && bits) {
                                    memcpy(bits, bytes.data(), stride * bmpH);
                                    HBITMAP old = nullptr;
                                    {
                                        std::lock_guard<std::mutex> lk(g_FlyoutArtMutex);
                                        old = g_FlyoutArtHBitmap;
                                        g_FlyoutArtHBitmap = hbm;
                                        g_FlyoutArtBmpW    = (int)bmpW;
                                        g_FlyoutArtBmpH    = (int)bmpH;
                                    }
                                    if (old) DeleteObject(old);
                                    if (HWND fly = g_FlyoutHwnd.load()) PostMessageW(fly, WM_FLYOUT_UPDATE, 0, 0);
                                }
                            }
                        }
                    } WH_CATCH(L"ApplyStateToWidget/FlyoutArt")
                } WH_CATCH(L"ApplyStateToWidget/LoadArt")
            }(weakArt, ref, version);
        }
    }

    // Apply system-integrated Acrylic background (only once; brush persists across updates).
    {
        auto wRoot = FindByName<Grid>(widget, kWidgetRootName);
        if (wRoot && !wRoot.Background().try_as<AcrylicBrush>()) {
            try {
                AcrylicBrush acrylic;
                acrylic.BackgroundSource(AcrylicBackgroundSource::HostBackdrop);
                acrylic.TintColor(ColorHelper::FromArgb(0xFF, 0x1A, 0x1A, 0x1A));
                acrylic.TintOpacity(0.6);
                wRoot.Background(acrylic);
            } catch (...) {
                wRoot.Background(MakeBrush(0xCC, 0x1A, 0x1A, 0x1A));
            }
        }
    }

    // SC-KV-4: progress bar (Rectangle fill, width driven by position ratio)
    if (auto track = FindByName<Grid>(widget, kProgressTrackName)) {
        bool show = g_Settings.showProgress && hasMedia && durationMs > 0;
        track.Visibility(show ? Visibility::Visible : Visibility::Collapsed);
        track.Background(MakeBrush(0x33, fgHi, fgHi, fgHi));  // subtle rail
        if (show) {
            if (auto fill = FindByName<Shapes::Rectangle>(track, kProgressFillName)) {
                fill.Fill(MakeBrush(0xCC, fgHi, fgHi, fgHi));  // bright fill
                double ratio = std::clamp(positionMs / (double)durationMs, 0.0, 1.0);
                fill.Width(ratio * track.ActualWidth());
                // Sync timer baseline so interpolation continues from this SMTC-reported
                // position rather than from whenever the timer originally started.
                g_ProgressLastTickMs = GetTickCount64();
            }
        }
    }

    if (auto tsTb = FindByName<TextBlock>(widget, kTimestampName)) {
        bool showTs = g_Settings.showProgress && hasMedia && durationMs > 0;
        tsTb.Visibility(showTs ? Visibility::Visible : Visibility::Collapsed);
        if (showTs) {
            bool hasHours = durationMs >= 3'600'000LL;
            tsTb.Text(FormatMs(positionMs, hasHours) + L" / " + FormatMs(durationMs));
            tsTb.Foreground(MakeBrush(0xB3, fgHi, fgHi, fgHi));
        }
    }

    // SC-HT-4: manage the interpolation timer based on playback state.
    // Only (re)start when not already running — avoids tearing it down on every
    // state update (play/pause toggle, color change, etc.) and resetting the
    // elapsed-time baseline unnecessarily.
    if (isPlaying && durationMs > 0) {
        if (!g_ProgressTimer) {
            g_ProgressLastTickMs = GetTickCount64();
            StartProgressTimer(widget);
        }
    } else {
        StopProgressTimer();
    }

    // SC-SP-4: fade widget in or out instead of snapping visibility.
    SetWidgetVisible(widget, hasMedia);
}

static void RefreshWidgetUI() {
    Grid widget{ nullptr };
    {
        std::lock_guard<std::mutex> g(g_WidgetMutex);
        widget = g_WidgetRoot.get();
    }
    if (!widget) return;
    try {
        auto weak = make_weak(widget);
        widget.Dispatcher().RunAsync(
            Windows::UI::Core::CoreDispatcherPriority::Normal,
            [weak]() {
                if (auto w = weak.get()) ApplyStateToWidget(w);
            });
    } WH_CATCH(L"RefreshWidgetUI/dispatch")
}

// ---------- Injection ----------
static void InjectWidgetInto(Grid rootGrid) {
    if (!rootGrid) return;

    // Already injected?
    auto existing = FindByName<Grid>(rootGrid, kWidgetRootName);
    if (existing) {
        {
            std::lock_guard<std::mutex> g(g_WidgetMutex);
            g_WidgetRoot = make_weak(existing);
            g_RootGrid   = make_weak(rootGrid);
        }
        ApplyStateToWidget(existing);
        // Signal even on the "already present" path — the GSMTC thread waits
        // on this event and the widget is ready either way.
        if (HANDLE ev = g_GsmtcStartEvent.load()) SetEvent(ev);
        return;
    }

    // Locate the system tray so we can right-anchor against it.
    FrameworkElement tray{ nullptr };
    int childCount = (int)rootGrid.Children().Size();
    for (int i = 0; i < childCount; ++i) {
        auto fe = rootGrid.Children().GetAt(i).try_as<FrameworkElement>();
        if (!fe) continue;
        std::wstring name = std::wstring(fe.Name());
        if (name == kSystemTrayGridName) {
            tray = fe;
        }
    }

    auto widget = BuildWidget();
    rootGrid.Children().Append(widget);

    {
        std::lock_guard<std::mutex> g(g_WidgetMutex);
        g_WidgetRoot   = make_weak(widget);
        g_RootGrid     = make_weak(rootGrid);
        g_SystemTray   = tray ? make_weak(tray) : weak_ref<FrameworkElement>{ nullptr };
        // Detach old tray resize subscription if present.
        if (tray && g_TrayResizeToken.value) {
            try { tray.SizeChanged(g_TrayResizeToken); } catch (...) {}
            g_TrayResizeToken = {};
        }
    }
    g_hTaskbarWnd.store(FindWindowW(L"Shell_TrayWnd", nullptr));

    // In Right mode the widget must stay adjacent to the system tray, so
    // subscribe to tray resize to update the margin whenever icon count changes.
    // Left and Center modes use a fixed offset unrelated to tray width.
    if (tray && g_Settings.widgetPosition == WidgetPosition::Right) {
        g_TrayResizeToken = tray.SizeChanged(
            [](IInspectable const&, SizeChangedEventArgs const&) {
                UpdateWidgetMargin();
            });
    }

    UpdateWidgetMargin();  // set initial margin
    // Re-run once the first layout pass completes so ActualWidth() is valid.
    widget.Loaded([](IInspectable const&, RoutedEventArgs const&) {
        UpdateWidgetMargin();
    });
    ApplyStateToWidget(widget);

    if (HANDLE ev = g_GsmtcStartEvent.load()) SetEvent(ev);
}

static void ScheduleScanAsync(FrameworkElement startNode) {
    if (!startNode) return;
    if (g_Unloading.load()) return;
    bool expected = false;
    if (!g_ScanPending.compare_exchange_strong(expected, true)) return;

    auto weak = make_weak(startNode);
    try {
        startNode.Dispatcher().RunAsync(
            Windows::UI::Core::CoreDispatcherPriority::Low,
            [weak]() {
                g_ScanPending = false;
                if (g_Unloading.load()) return;
                auto node = weak.get();
                if (!node) return;
                try {
                    auto frame = WalkUpToTaskbarFrame(node);
                    if (!frame) return;
                    auto rootGrid = FindRootGrid(frame);
                    if (!rootGrid) return;
                    InjectWidgetInto(rootGrid);
                } catch (...) {
                    Wh_Log(L"[inject] Exception during XAML tree walk");
                }
            });
    } catch (...) {
        g_ScanPending = false;
        Wh_Log(L"[inject] Exception scheduling on dispatcher");
    }
}

static void RemoveWidget() {
    Grid widget{ nullptr };
    Grid rootGrid{ nullptr };
    {
        std::lock_guard<std::mutex> g(g_WidgetMutex);
        widget = g_WidgetRoot.get();
        rootGrid = g_RootGrid.get();
        g_WidgetRoot = nullptr;
        g_RootGrid = nullptr;
    }
    if (!widget || !rootGrid) return;

    try {
        auto weakGrid  = make_weak(rootGrid);
        auto weakWidget = make_weak(widget);
        rootGrid.Dispatcher().RunAsync(
            Windows::UI::Core::CoreDispatcherPriority::Normal,
            [weakGrid, weakWidget]() {
                // All XAML cleanup must happen on the dispatcher thread.
                StopMarquee();
                StopProgressTimer();
                if (g_TextFadeStoryboard) {
                    try { g_TextFadeStoryboard.Stop(); } catch (...) {}
                    g_TextFadeStoryboard = nullptr;
                }
                if (g_WidgetFadeStoryboard) {
                    try { g_WidgetFadeStoryboard.Stop(); } catch (...) {}
                    g_WidgetFadeStoryboard = nullptr;
                }
                // Revoke SizeChanged token on the correct (UI) thread.
                if (g_TitleSizeChangedToken.value) {
                    if (auto w = weakWidget.get()) {
                        if (auto tb = FindByName<TextBlock>(w, kTitleName))
                            tb.SizeChanged(g_TitleSizeChangedToken);
                    }
                    g_TitleSizeChangedToken = {};
                }
                if (g_TrayResizeToken.value) {
                    if (auto tray = g_SystemTray.get())
                        try { tray.SizeChanged(g_TrayResizeToken); } catch (...) {}
                    g_TrayResizeToken = {};
                }
                if (HWND fly = g_FlyoutHwnd.load())
                    PostMessageW(fly, WM_FLYOUT_HIDE_NOW, 0, 0);
                auto g = weakGrid.get();
                if (!g) return;
                auto children = g.Children();
                for (int i = (int)children.Size() - 1; i >= 0; --i) {
                    auto el = children.GetAt(i).try_as<FrameworkElement>();
                    if (el && std::wstring(el.Name()) == kWidgetRootName) {
                        children.RemoveAt(i);
                    }
                }
            });
    } catch (...) {}
}

// ---------- GSMTC ----------

static winrt::fire_and_forget UpdateTimelineAsync(int idx) {
    g_AsyncTasks++;
    struct AsyncTaskGuard { ~AsyncTaskGuard() { g_AsyncTasks--; } } taskGuard;

    GlobalSystemMediaTransportControlsSession session{ nullptr };
    {
        std::lock_guard<std::mutex> lk(g_MediaMutex);
        if (g_Unloading.load() || idx < 0 || idx >= g_MediaStateCount) co_return;
        session = g_MediaStates[idx].session;
    }
    if (!session) co_return;

    int64_t posMs = 0, durMs = 0;
    try {
        auto tl = session.GetTimelineProperties();
        if (tl) {
            auto pos   = tl.Position();
            auto end   = tl.EndTime();
            auto start = tl.StartTime();
            durMs = (end - start).count() / 10'000;
            posMs = pos.count() / 10'000;
        }
    } WH_CATCH(L"UpdateTimelineAsync")

    {
        std::lock_guard<std::mutex> lk(g_MediaMutex);
        if (g_Unloading.load() || idx >= g_MediaStateCount) co_return;
        if (g_MediaStates[idx].session != session) co_return;
        auto& m = g_MediaStates[idx];
        if (m.positionMs == posMs && m.durationMs == durMs) co_return;
        m.positionMs = posMs;
        m.durationMs = durMs;
    }
    RefreshWidgetUI();
}

static winrt::fire_and_forget UpdateOneSessionAsync(int idx) {
    g_AsyncTasks++;
    struct AsyncTaskGuard { ~AsyncTaskGuard() { g_AsyncTasks--; } } taskGuard;

    GlobalSystemMediaTransportControlsSession session{ nullptr };
    SessionSource source = SessionSource::Unknown;
    std::wstring sessionId;
    {
        std::lock_guard<std::mutex> lk(g_MediaMutex);
        if (g_Unloading.load() || idx < 0 || idx >= g_MediaStateCount) co_return;
        session   = g_MediaStates[idx].session;
        source    = g_MediaStates[idx].source;
        sessionId = g_MediaStates[idx].sessionId;
    }
    if (!session) co_return;

    // Libby runs as a Chrome extension (AUMID Chrome._crx_bbcjjjnjadekjghhbjddadjgfc).
    // SMTC exposes Chrome's own icon; override with the embedded Libby icon instead.
    static constexpr std::wstring_view kLibbyAumid = L"Chrome._crx_bbcjjjnjadekjghhbjddadjgfc";
    const bool isLibby = (sessionId == kLibbyAumid);

    std::wstring title, artist;
    bool playing = false;
    double playbackRate = 1.0;
    bool canSkipForward = false, canSkipBackward = false;
    IRandomAccessStreamReference newThumbRef{ nullptr };
    int64_t posMs = 0, durMs = 0;
    try {
        auto props = co_await session.TryGetMediaPropertiesAsync();
        if (props) {
            title  = props.Title().c_str();
            artist = props.Artist().c_str();

            // 2a: Audiobook fallback — prefer AlbumTitle when Title is empty
            // (Libby and some audiobook apps set chapter name in Title and book
            // name in AlbumTitle; AlbumArtist carries the author when Artist is
            // blank). Safe for music sessions: AlbumTitle is typically empty or
            // identical to Title, so the guard prevents any change.
            auto albumTitle  = std::wstring(props.AlbumTitle().c_str());
            auto albumArtist = std::wstring(props.AlbumArtist().c_str());
            if (title.empty()  && !albumTitle.empty())  title  = albumTitle;
            if (artist.empty() && !albumArtist.empty()) artist = albumArtist;
            if (title.empty()) title = L"\u266B";

            // Browser title cleanup: strip leading "AppName - " prefix that
            // browsers inject from the tab title (e.g. "Libby - Open: ...").
            if (source == SessionSource::Browser || source == SessionSource::BrowserFirefox) {
                auto dash = title.find(L" - ");
                if (dash != std::wstring::npos && dash > 0 && dash < 32)
                    title.erase(0, dash + 3);  // drop "AppName - "
                // Libby tab titles follow with "Open: <book title>"
                if (title.size() > 6 && title.compare(0, 6, L"Open: ") == 0)
                    title.erase(0, 6);
            }

            // Artist cleanup: strip whitespace then clear if purely numeric —
            // meaningless metadata noise from browser sessions (Libby sends
            // chapter index as artist, sometimes with surrounding spaces).
            if (source == SessionSource::Browser || source == SessionSource::BrowserFirefox) {
                auto ltrim = artist.find_first_not_of(L" \t\r\n");
                auto rtrim = artist.find_last_not_of(L" \t\r\n");
                if (ltrim == std::wstring::npos) {
                    artist.clear();
                } else {
                    artist = artist.substr(ltrim, rtrim - ltrim + 1);
                    if (artist.find_first_not_of(L"0123456789") == std::wstring::npos)
                        artist.clear();
                }
            }
        }

        // Fetch thumbnail stream reference (null for Libby and apps with no art)
        try {
            if (props && !isLibby) newThumbRef = props.Thumbnail();
        } WH_CATCH(L"UpdateOneSessionAsync/Thumbnail")

        // For Libby, override the SMTC thumbnail (Chrome's own icon) with the
        // embedded Libby icon written to a temp file on init.
        if (isLibby && !g_LibbyIconTempPath.empty()) {
            try {
                auto file = co_await StorageFile::GetFileFromPathAsync(g_LibbyIconTempPath);
                newThumbRef = RandomAccessStreamReference::CreateFromFile(file);
            } WH_CATCH(L"UpdateOneSessionAsync/LibbyIcon")
        }
        auto pb = session.GetPlaybackInfo();
        if (pb) {
            playing = (pb.PlaybackStatus()
                == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing);
            // 2b: Playback rate (IReference<double> — null when not supported)
            if (auto rateRef = pb.PlaybackRate()) playbackRate = rateRef.Value();
            // 2c: Skip forward / backward capability flags
            if (auto ctrls = pb.Controls()) {
                canSkipForward  = ctrls.IsNextEnabled();
                canSkipBackward = ctrls.IsPreviousEnabled();
            }
        }
    } WH_CATCH(L"UpdateOneSessionAsync")

    try {
        auto tl = session.GetTimelineProperties();
        if (tl) {
            auto pos   = tl.Position();
            auto end   = tl.EndTime();
            auto start = tl.StartTime();
            posMs = pos.count() / 10'000;
            durMs = (end - start).count() / 10'000;  // 100ns → ms
        }
    } WH_CATCH(L"UpdateOneSessionAsync/Timeline")

    // Audiobook detection: > 1 hour is the primary signal. A chapter keyword in
    // the title ("Chapter N") combined with > 15 minutes catches short audiobook
    // chapters (common in Libby and Audiobookshelf) that would otherwise fall
    // through as NativeApp music. "Part " is intentionally excluded — too many
    // false positives from music tracks (concept albums, classical works).
    bool hasChapterKeyword = (title.find(L"Chapter ") != std::wstring::npos);
    bool isAb = (durMs > 3'600'000LL) ||
                (durMs > 900'000LL && hasChapterKeyword);

    // Build formatted position string: HH:MM:SS when hours present, else MM:SS.
    std::wstring posFmt = FormatMs(posMs);

    {
        std::lock_guard<std::mutex> lk(g_MediaMutex);
        if (g_Unloading.load() || idx >= g_MediaStateCount) co_return;
        if (g_MediaStates[idx].session != session) co_return;

        auto& m = g_MediaStates[idx];
        m.positionMs = posMs;
        m.durationMs = durMs;
        if (m.title == title && m.artist == artist && m.isPlaying == playing
            && std::fabs(m.playbackRate - playbackRate) < 0.001
            && m.canSkipForward == canSkipForward
            && m.canSkipBackward == canSkipBackward
            && m.isAudiobook == isAb
            && m.positionFormatted == posFmt) co_return;
        m.title          = title;
        m.artist         = artist;
        m.isPlaying      = playing;
        m.playbackRate   = playbackRate;
        m.canSkipForward  = canSkipForward;
        m.canSkipBackward = canSkipBackward;
        m.isAudiobook        = isAb;
        m.positionFormatted  = posFmt;

        // Thumbnail: bump version whenever art identity changes (COM pointer comparison).
        bool artChanged = (m.thumbnailRef != newThumbRef);
        if (artChanged) {
            m.thumbnailRef    = newThumbRef;
            m.thumbnailVersion++;
        }
    }
    RefreshWidgetUI();
}

struct SessionCleanupData {
    GlobalSystemMediaTransportControlsSession session{ nullptr };
    event_token propsChangedToken{};
    event_token playbackChangedToken{};
    event_token timelineChangedToken{};
};

static SessionCleanupData DetachSessionLocked(int idx) {
    SessionCleanupData cleanup;
    if (idx < 0 || idx >= g_MediaStateCount) return cleanup;
    auto& m = g_MediaStates[idx];
    cleanup.session = m.session;
    cleanup.propsChangedToken = m.propsChangedToken;
    cleanup.playbackChangedToken = m.playbackChangedToken;
    cleanup.timelineChangedToken = m.timelineChangedToken;
    m = MediaState{};
    return cleanup;
}

static void PerformSessionCleanup(const SessionCleanupData& cleanup) {
    if (!cleanup.session) return;
    try {
        if (cleanup.propsChangedToken.value)    cleanup.session.MediaPropertiesChanged(cleanup.propsChangedToken);
        if (cleanup.playbackChangedToken.value) cleanup.session.PlaybackInfoChanged(cleanup.playbackChangedToken);
        if (cleanup.timelineChangedToken.value) cleanup.session.TimelinePropertiesChanged(cleanup.timelineChangedToken);
    } catch (...) {}
}

static void DoEnumerateAndRefresh() {
    GlobalSystemMediaTransportControlsSessionManager mgr{ nullptr };
    {
        std::lock_guard<std::mutex> lk(g_MediaMutex);
        if (g_Unloading.load()) return;
        mgr = g_SessionManager;
    }
    if (!mgr) return;

    auto sessions = [&]() {
        try { return mgr.GetSessions(); }
        catch (...) { return decltype(mgr.GetSessions()){ nullptr }; }
    }();
    if (!sessions) { Wh_Log(L"[gsmtc] GetSessions failed"); return; }

    SessionCleanupData cleanupList[MAX_SESSIONS];
    int cleanupCount = 0;
    {
        std::lock_guard<std::mutex> lk(g_MediaMutex);
        if (g_Unloading.load()) return;
        cleanupCount = g_MediaStateCount;
        for (int i = 0; i < g_MediaStateCount; ++i) {
            cleanupList[i] = DetachSessionLocked(i);
        }
        g_MediaStateCount = 0;

        int playingIdx = -1;
        for (auto const& s : sessions) {
            if (g_MediaStateCount >= MAX_SESSIONS) break;
            auto& m = g_MediaStates[g_MediaStateCount];
            m.session = s;
            m.sessionId = WH_TRY_OR(std::wstring(s.SourceAppUserModelId().c_str()), std::wstring(L""));
            m.source    = ClassifySessionSource(m.sessionId);
            try {
                m.propsChangedToken = s.MediaPropertiesChanged(
                    [idx = g_MediaStateCount](auto&&, auto&&) { UpdateOneSessionAsync(idx); });
            } WH_CATCH(L"DoEnum/MediaPropertiesChanged")
            try {
                m.playbackChangedToken = s.PlaybackInfoChanged(
                    [idx = g_MediaStateCount](auto&&, auto&&) { UpdateOneSessionAsync(idx); });
            } WH_CATCH(L"DoEnum/PlaybackInfoChanged")
            try {
                m.timelineChangedToken = s.TimelinePropertiesChanged(
                    [idx = g_MediaStateCount](auto&&, auto&&) { UpdateTimelineAsync(idx); });
            } WH_CATCH(L"DoEnum/TimelinePropertiesChanged")
            try {
                auto pb = s.GetPlaybackInfo();
                if (pb && pb.PlaybackStatus()
                        == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing) {
                    if (playingIdx == -1) playingIdx = g_MediaStateCount;
                }
            } WH_CATCH(L"DoEnum/GetPlaybackInfo")
            g_MediaStateCount++;
        }
        g_ActiveSessionIndex = (playingIdx >= 0) ? playingIdx : 0;
    }

    for (int i = 0; i < cleanupCount; ++i) {
        PerformSessionCleanup(cleanupList[i]);
    }

    int n;
    { std::lock_guard<std::mutex> lk(g_MediaMutex); n = g_MediaStateCount; }
    for (int i = 0; i < n; ++i) UpdateOneSessionAsync(i);
    RefreshWidgetUI();
}

static DWORD WINAPI GsmtcThreadFunc(LPVOID) {
    // Guard against the narrow race where the thread is created just as uninit begins.
    if (g_Unloading.load()) return 0;
    bool staOk = false;
    try {
        init_apartment(apartment_type::single_threaded);
        staOk = true;
    } WH_CATCH(L"GsmtcThread/STA")
    if (!staOk) return 0;

    if (HANDLE ev = g_GsmtcStartEvent.load()) WaitForSingleObject(ev, INFINITE);

    if (g_Unloading.load()) return 0;

    IAsyncOperation<GlobalSystemMediaTransportControlsSessionManager> req{ nullptr };
    try {
        req = GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
    } catch (winrt::hresult_error const& e) {
        Wh_Log(L"[gsmtc] thread: RequestAsync() threw hr=0x%08X %s", (unsigned)e.code(), e.message().c_str());
        return 0;
    } catch (...) {
        Wh_Log(L"[gsmtc] thread: RequestAsync() threw unknown exception");
        return 0;
    }

    {
        std::lock_guard<std::mutex> lk(g_MediaMutex);
        if (g_Unloading.load()) { req.Cancel(); return 0; }
        g_PendingRequest = req;
    }

    req.Completed([req](IAsyncOperation<GlobalSystemMediaTransportControlsSessionManager> const&,
                        AsyncStatus status) {
        {
            std::lock_guard<std::mutex> lk(g_MediaMutex);
            g_PendingRequest = nullptr;
        }

        if (status == AsyncStatus::Error) {
            try {
                req.GetResults();
            } catch (winrt::hresult_error const& e) {
                Wh_Log(L"[gsmtc] Completed: error hr=0x%08X %s", (unsigned)e.code(), e.message().c_str());
            } catch (...) {}
            return;
        }
        if (status != AsyncStatus::Completed || g_Unloading.load()) return;

        GlobalSystemMediaTransportControlsSessionManager mgr{ nullptr };
        try {
            mgr = req.GetResults();
        } catch (winrt::hresult_error const& e) {
            Wh_Log(L"[gsmtc] GetResults threw hr=0x%08X %s", (unsigned)e.code(), e.message().c_str());
            return;
        } catch (...) {
            Wh_Log(L"[gsmtc] GetResults threw unknown exception");
            return;
        }
        if (!mgr) { Wh_Log(L"[gsmtc] manager is null after completion"); return; }

        {
            std::lock_guard<std::mutex> lk(g_MediaMutex);
            if (g_Unloading.load()) return;
            g_SessionManager = mgr;
        }
        try {
            g_SessionsChangedToken = g_SessionManager.SessionsChanged(
                [](auto&&, auto&&) { DoEnumerateAndRefresh(); });
        } WH_CATCH(L"GsmtcThread/SessionsChanged")

        DoEnumerateAndRefresh();
    });

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_QUIT) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

// ---------- Fullscreen polling ----------

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

static bool IsForegroundWindowFullscreen(HMONITOR hTaskbarMon) {
    if (!hTaskbarMon) return false;
    HWND hFore = GetForegroundWindow();
    if (!hFore) return false;
    HMONITOR hMon = MonitorFromWindow(hFore, MONITOR_DEFAULTTONEAREST);
    if (!hMon || hMon != hTaskbarMon) return false;
    MONITORINFO mi{ sizeof(mi) };
    if (!GetMonitorInfoW(hMon, &mi)) return false;
    RECT wr{};
    if (!GetWindowRect(hFore, &wr)) return false;
    return wr.left  <= mi.rcMonitor.left  &&
           wr.top   <= mi.rcMonitor.top   &&
           wr.right >= mi.rcMonitor.right &&
           wr.bottom >= mi.rcMonitor.bottom;
}

static DWORD WINAPI FullscreenPollThread(LPVOID) {
    bool lastHide = false;
    while (WaitForSingleObject(g_PollStop, 1000) == WAIT_TIMEOUT) {
        if (!g_Settings.hideFullscreen) continue;
        HWND hTaskbar = g_hTaskbarWnd.load();
        HMONITOR hTaskbarMon = hTaskbar
            ? MonitorFromWindow(hTaskbar, MONITOR_DEFAULTTONEAREST)
            : nullptr;
        QUERY_USER_NOTIFICATION_STATE state{};
        if (FAILED(SHQueryUserNotificationState(&state))) continue;
        // QUNS_BUSY is intentionally excluded — it fires during lock screen,
        // display wake transitions, and dialogs. Only hide for genuine
        // fullscreen states where the taskbar itself would be hidden.
        // QUNS_PRESENTATION_MODE is a global system-wide state (no per-monitor
        // info available), so it always triggers hide. QUNS_RUNNING_D3D_FULL_SCREEN
        // is gated by monitor: only hide if the D3D app is on the taskbar's monitor.
        bool hide = !IsTaskbarEffectivelyVisible(hTaskbar)
                 || (state == QUNS_PRESENTATION_MODE)
                 || IsForegroundWindowFullscreen(hTaskbarMon);

        // Only dispatch when the hide state actually changes. Calling
        // ApplyStateToWidget every second on an idle widget races with in-flight
        // fade storyboards and causes intermittent flicker. GSMTC events keep the
        // widget content current; the poll only needs to react to transitions.
        if (hide == lastHide) continue;
        lastHide = hide;

        Grid widget{ nullptr };
        {
            std::lock_guard<std::mutex> g(g_WidgetMutex);
            widget = g_WidgetRoot.get();
        }
        if (!widget) continue;
        try {
            auto weak = make_weak(widget);
            widget.Dispatcher().RunAsync(
                Windows::UI::Core::CoreDispatcherPriority::Low,
                [weak, hide]() {
                    if (auto w = weak.get()) {
                        if (hide) {
                            // SC-SP-4: fade out instead of snap-collapse.
                            SetWidgetVisible(w, false);
                        } else {
                            // Transitioning back from fullscreen hide — do a full
                            // state refresh so content reflects any changes that
                            // occurred while the widget was hidden.
                            ApplyStateToWidget(w);
                        }
                    }
                });
        } WH_CATCH(L"FullscreenPoll/dispatch")
    }
    return 0;
}

// ---------- Hooks ----------
using TaskListButton_UpdateVisualStates_t = void(WINAPI*)(void*);
static TaskListButton_UpdateVisualStates_t TaskListButton_UpdateVisualStates_Original = nullptr;
static void WINAPI TaskListButton_UpdateVisualStates_Hook(void* pThis) {
    // RAII guard so the counter is always decremented even if the original throws.
    struct HookGuard { ~HookGuard() { g_HookCallCounter--; } } guard;
    g_HookCallCounter++;
    TaskListButton_UpdateVisualStates_Original(pThis);
    if (!g_Unloading.load()) {
        auto elem = GetFrameworkElementFromNative(pThis);
        if (elem) ScheduleScanAsync(elem);
    }
}

static bool HookTaskbarViewDllSymbols(HMODULE module) {
    WindhawkUtils::SYMBOL_HOOK hooks[] = {
        {
            { L"private: void __cdecl winrt::Taskbar::implementation::TaskListButton::UpdateVisualStates(void)" },
            (void**)&TaskListButton_UpdateVisualStates_Original,
            (void*)TaskListButton_UpdateVisualStates_Hook,
            false,
        },
    };
    if (!WindhawkUtils::HookSymbols(module, hooks, ARRAYSIZE(hooks))) {
        Wh_Log(L"HookSymbols(Taskbar.View.dll) failed");
        return false;
    }
    return true;
}

// Post WM_SIZE to Shell_TrayWnd (and secondary bars) to trigger UpdateVisualStates
// without waiting for user interaction. Called after hooks are applied.
static void TriggerInitialScan() {
    g_InitialScanThread = std::thread([]() {
        // Poll up to 30 s for Shell_TrayWnd. This does two things:
        // 1. Delays the WM_SIZE injection trigger until the taskbar exists.
        // 2. Delays GSMTC initialization until the shell — and the WinRT media
        //    transport service that comes with it — are actually ready. Signalling
        //    GSMTC at 200 ms into a cold boot when Shell_TrayWnd doesn't exist
        //    yet causes RequestAsync to succeed against an uninitialized service
        //    stub; the Completed callback fires ~2 min later in a bad state and
        //    crashes Explorer with an unhandled hardware fault.
        HWND hTray = nullptr;
        for (int i = 0; i < 300 && !g_Unloading.load(); ++i) {
            Sleep(100);
            hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
            if (hTray) break;
        }
        if (g_Unloading.load()) return;

        if (hTray) {
            RECT rc{};
            GetClientRect(hTray, &rc);
            PostMessageW(hTray, WM_SIZE, SIZE_RESTORED, MAKELPARAM(rc.right, rc.bottom));
        }
        HWND hTray2 = FindWindowW(L"Shell_SecondaryTrayWnd", nullptr);
        while (hTray2) {
            PostMessageW(hTray2, WM_SIZE, SIZE_RESTORED, 0);
            hTray2 = FindWindowExW(nullptr, hTray2, L"Shell_SecondaryTrayWnd", nullptr);
        }
        // Signal the GSMTC thread only after the shell is ready.
        if (HANDLE ev = g_GsmtcStartEvent.load()) SetEvent(ev);
    });
}

// Polls for Taskbar.View.dll (or ExplorerExtensions.dll on older builds) every
// 100 ms for up to 60 s, then installs the UpdateVisualStates hook. Using
// GetModuleHandleW polling instead of a LoadLibraryExW hook avoids the
// cold-start trampoline race: on boot Explorer loads dozens of DLLs on multiple
// threads simultaneously, and patching LoadLibraryExW while another thread is
// executing it causes an unhandled hardware fault that kills Explorer.
static void PollForTaskbarViewDll() {
    g_PollForDllThread = std::thread([]() {
        for (int i = 0; i < 600 && !g_Unloading.load(); ++i) {
            Sleep(100);
            HMODULE m = GetModuleHandleW(L"Taskbar.View.dll");
            if (!m) m = GetModuleHandleW(L"ExplorerExtensions.dll");
            if (!m) continue;

            bool already = g_TaskbarViewDllLoaded.exchange(true);
            if (already) break;

            if (!HookTaskbarViewDllSymbols(m)) break;
            Wh_ApplyHookOperations();
            // All deferred initialization: hooks are installed and XAML is confirmed
            // loaded, so it is safe to create threads that touch COM/WinRT state.
            if (!g_Unloading.load()) {
                g_PollStop   = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                g_PollThread = CreateThread(nullptr, 0, FullscreenPollThread, nullptr, 0, nullptr);
                g_GsmtcStartEvent.store(CreateEventW(nullptr, TRUE, FALSE, nullptr));
                g_GsmtcThread = CreateThread(nullptr, 0, GsmtcThreadFunc, nullptr, 0, &g_GsmtcThreadId);
            }
            if (!g_Unloading.load()) {
                TriggerInitialScan();
            }
            break;
        }
    });
}

// ---------- Mod entry points ----------
BOOL Wh_ModInit() {
    LoadSettings();

    // Write the embedded Libby icon to a temp file so it can be opened as a
    // StorageFile stream reference for the art pipeline (widget + flyout).
    {
        wchar_t tmp[MAX_PATH];
        if (GetTempPathW(MAX_PATH, tmp)) {
            std::wstring path = std::wstring(tmp) + L"wh_libby_icon.png";
            HANDLE hf = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hf != INVALID_HANDLE_VALUE) {
                DWORD written = 0;
                WriteFile(hf, kLibbyIconPng, sizeof(kLibbyIconPng), &written, nullptr);
                CloseHandle(hf);
                if (written == sizeof(kLibbyIconPng)) g_LibbyIconTempPath = path;
            }
        }
    }

    HMODULE taskbarView = GetModuleHandleW(L"Taskbar.View.dll");
    if (!taskbarView) taskbarView = GetModuleHandleW(L"ExplorerExtensions.dll");

    // Start flyout thread unconditionally; it waits for WM_FLYOUT_SHOW messages.
    {
        HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        g_FlyoutThread = CreateThread(nullptr, 0, FlyoutThreadProc, ready, 0, &g_FlyoutThreadId);
        if (g_FlyoutThread) WaitForSingleObject(ready, 2000);
        CloseHandle(ready);
    }

    if (taskbarView) {
        // Direct path: DLL already loaded. Initialize everything immediately.
        g_TaskbarViewDllLoaded = true;
        if (!HookTaskbarViewDllSymbols(taskbarView)) return FALSE;

        g_PollStop   = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        g_PollThread = CreateThread(nullptr, 0, FullscreenPollThread, nullptr, 0, nullptr);

        g_GsmtcStartEvent.store(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        g_GsmtcThread = CreateThread(nullptr, 0, GsmtcThreadFunc, nullptr, 0, &g_GsmtcThreadId);

        TriggerInitialScan();
    } else {
        // Cold-start path: DLL not yet loaded. Create exactly one poll thread and
        // return immediately. PollForTaskbarViewDll creates all other threads after
        // the DLL appears and hooks are applied — avoiding any thread creation or
        // COM initialization during Explorer's hazardous early-boot window.
        PollForTaskbarViewDll();
    }

    return TRUE;
}

void Wh_ModUninit() {
    g_Unloading = true;
    if (g_PollForDllThread.joinable()) g_PollForDllThread.join();
    if (g_InitialScanThread.joinable()) g_InitialScanThread.join();

    if (!g_LibbyIconTempPath.empty()) DeleteFileW(g_LibbyIconTempPath.c_str());

    // Shut down the flyout window and its thread before any other teardown.
    if (HWND fly = g_FlyoutHwnd.load()) PostMessageW(fly, WM_FLYOUT_QUIT, 0, 0);
    if (g_FlyoutThread) {
        WaitForSingleObject(g_FlyoutThread, 2000);
        CloseHandle(g_FlyoutThread);
        g_FlyoutThread = nullptr;
    }
    // Free flyout art bitmap (thread is already stopped so no lock needed).
    if (g_FlyoutArtHBitmap) { DeleteObject(g_FlyoutArtHBitmap); g_FlyoutArtHBitmap = nullptr; }

    if (g_PollStop) SetEvent(g_PollStop);
    if (HANDLE ev = g_GsmtcStartEvent.load()) SetEvent(ev);
    if (g_GsmtcThreadId) PostThreadMessageW(g_GsmtcThreadId, WM_QUIT, 0, 0);

    if (g_PollThread) {
        WaitForSingleObject(g_PollThread, 2000);
        CloseHandle(g_PollThread);
        g_PollThread = nullptr;
    }
    if (g_GsmtcThread) {
        WaitForSingleObject(g_GsmtcThread, 2000);
        CloseHandle(g_GsmtcThread);
        g_GsmtcThread = nullptr;
    }
    if (g_PollStop) { CloseHandle(g_PollStop); g_PollStop = nullptr; }
    if (HANDLE ev = g_GsmtcStartEvent.exchange(nullptr)) CloseHandle(ev);

    SessionCleanupData cleanupList[MAX_SESSIONS];
    int cleanupCount = 0;
    GlobalSystemMediaTransportControlsSessionManager mgr{ nullptr };
    event_token mgrToken{};
    {
        std::lock_guard<std::mutex> g(g_MediaMutex);
        if (g_PendingRequest) {
            try { g_PendingRequest.Cancel(); } catch (...) {}
        }
        mgr = g_SessionManager;
        mgrToken = g_SessionsChangedToken;
        g_SessionsChangedToken = {};
        g_SessionManager = nullptr;
        
        cleanupCount = g_MediaStateCount;
        for (int i = 0; i < g_MediaStateCount; ++i) {
            cleanupList[i] = DetachSessionLocked(i);
        }
        g_MediaStateCount = 0;
    }

    try {
        if (mgr && mgrToken.value) {
            mgr.SessionsChanged(mgrToken);
        }
    } catch (...) {}

    for (int i = 0; i < cleanupCount; ++i) {
        PerformSessionCleanup(cleanupList[i]);
    }

    RemoveWidget();

    // Spin until in-flight hooks finish.
    for (int i = 0; i < 50 && g_HookCallCounter.load() > 0; ++i) Sleep(100);

    // Spin until in-flight async tasks finish (50 × 100 ms = 5 s, matching hook drain).
    for (int i = 0; i < 50 && g_AsyncTasks.load() > 0; ++i) Sleep(100);
    if (g_AsyncTasks.load() > 0) {
        Wh_Log(L"[uninit] WARNING: async task drain timed out (%d tasks remaining)",
               g_AsyncTasks.load());
    }
}

void Wh_ModSettingsChanged() {
    LoadSettings();
    if (HWND fly = g_FlyoutHwnd.load())
        PostMessageW(fly, WM_FLYOUT_SETTINGS, 0, 0);
    Grid widget{ nullptr };
    {
        std::lock_guard<std::mutex> g(g_WidgetMutex);
        widget = g_WidgetRoot.get();
    }
    if (!widget) return;
    try {
        auto weak = make_weak(widget);
        int w = g_Settings.panelWidth, h = g_Settings.panelHeight;
        double fs = g_Settings.fontSize;
        HorizontalAlignment ha =
            g_Settings.widgetPosition == WidgetPosition::Left   ? HorizontalAlignment::Left   :
            g_Settings.widgetPosition == WidgetPosition::Center ? HorizontalAlignment::Center :
                                                                  HorizontalAlignment::Right;
        widget.Dispatcher().RunAsync(
            Windows::UI::Core::CoreDispatcherPriority::Normal,
            [weak, w, h, fs, ha]() {
                auto g = weak.get();
                if (!g) return;
                g.Width((double)w);
                g.HorizontalAlignment(ha);
                if (auto art = FindByName<Image>(g, kAlbumArtName)) {
                    art.Width((double)h);
                    art.Height((double)h);
                }
                if (auto t  = FindByName<TextBlock>(g, kTitleName))  t.FontSize(fs);
                if (auto t2 = FindByName<TextBlock>(g, kTitleName2)) t2.FontSize(fs);
                if (auto tc = FindByName<Canvas>(g, kTitleCanvasName)) {
                    tc.Height(fs * 1.6);
                    if (auto clip = tc.Clip().try_as<RectangleGeometry>()) {
                        auto r = clip.Rect();
                        clip.Rect(RectHelper::FromCoordinatesAndDimensions(
                            0, 0, r.Width, (float)(fs * 1.6)));
                    }
                }
                if (auto a = FindByName<TextBlock>(g, kArtistName)) a.FontSize(fs);
                if (auto ts = FindByName<TextBlock>(g, kTimestampName))
                    ts.FontSize(std::max(8.0, fs - 2.0));
                if (auto s = FindByName<TextBlock>(g, kSessionCountName)) s.FontSize(fs);
                ApplyStateToWidget(g);
                UpdateWidgetMargin();  // must run on UI thread
            });
    } catch (...) {}
}
