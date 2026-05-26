// ==WindhawkMod==
// @id              native-taskbar-media-controller
// @name            Native Taskbar Media Controller
// @description     Native XAML-injected media controller in the Windows 11 taskbar — shows now-playing info with playback controls.
// @version         1.4.7
// @author          StarlightDaemon
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lole32 -loleaut32 -lruntimeobject -luser32 -lwindowsapp -lversion -lshell32 -lgdi32 -ldwmapi -DWINVER=0x0A00 -Wl,--undefined=__imp_FindWindowW -Wl,--undefined=__imp_FindWindowExW -Wl,--undefined=__imp_PostMessageW -Wl,--undefined=__imp_GetClientRect
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Native Taskbar Media Controller

Injects a media controller natively into the Windows 11 taskbar XAML tree —
no overlay window, no separate process. The widget lives as a real child of
the taskbar's own UI, giving it correct z-ordering, auto-hide support, and
DPI handling automatically.

## Features

- **Now playing** — title and artist from any GSMTC-compatible app (Spotify,
  YouTube Music, Windows Media Player, browsers, audiobook apps, and more)
- **Playback controls** — play/pause toggle, skip-next, and skip-back buttons;
  skip-back is hidden for sources that don't support it
- **Position timestamp** — shows current position and total duration as
  `M:SS / M:SS` (or `H:MM:SS / H:MM:SS` for long tracks) when the source
  exposes timeline data; hidden automatically when timeline is unavailable
- **Multi-session** — a session count chip appears when multiple media apps
  are active; tap it to cycle between sessions
- **Audiobook mode** — tracks longer than one hour are treated as audiobooks:
  skip buttons navigate chapters, and the playback rate is shown next to the
  title when it differs from 1×
- **Fullscreen auto-hide** — panel collapses when a fullscreen app is
  detected; also hides when the taskbar slides off-screen in auto-hide mode
- **Adaptive text color** — text adjusts to light or dark based on the Windows theme
- **Double-click to focus** — double-click the widget to bring the source
  media app to the foreground (or minimize it if already focused)
- **Track progress bar** — a slim bar at the widget bottom shows playback
  position; paired with the timestamp display

## Compatibility notes

- **Browsers** (Chrome, Edge, Brave, Opera, Vivaldi, Arc, Thorium):
  one SMTC session per browser process; all tabs share it. Timeline data
  is not available from browser sessions.
- **Firefox**: media info is fully supported; timeline data is not available
  (Mozilla Bugzilla 1689538).
- **Audiobook apps**: Libby/OverDrive and similar apps that expose chapter
  navigation are fully supported. Audible's Windows app does not register
  SMTC sessions and is not supported.

## Requirements

- Windows 11 22H2 or later
- [Windhawk](https://windhawk.net) mod loader
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- PanelWidth: 300
  $name: Widget width (px)
- PanelHeight: 40
  $name: Widget height (px)
- FontSize: 11
  $name: Font size
- OffsetX: 8
  $name: Gap between widget and system tray (px)
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
}

// ---------- GSMTC multi-session state ----------
static constexpr int MAX_SESSIONS = 10;

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
static winrt::event_token   g_TitleSizeChangedToken{};
static HANDLE g_PollThread = nullptr;
static HANDLE g_PollStop = nullptr;
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

static HWND   g_FlyoutHwnd     = nullptr;
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

// Widget right-margin (trayWidth + offsetX, in DIPs) — updated in UpdateWidgetMargin.
// Flyout thread reads this to compute its screen position.
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

// Recompute the widget's right margin from the measured system tray width.
// Called at inject time and whenever the tray resizes.
static void UpdateWidgetMargin() {
    Grid widget{ nullptr };
    FrameworkElement tray{ nullptr };
    {
        std::lock_guard<std::mutex> g(g_WidgetMutex);
        widget = g_WidgetRoot.get();
        tray   = g_SystemTray.get();
    }
    if (!widget) return;

    double trayWidth = tray ? tray.ActualWidth() : 0.0;
    double gap       = (double)g_Settings.offsetX;
    double margin    = trayWidth + gap;
    widget.Margin(ThicknessHelper::FromLengths(0, 0, margin, 0));
    g_FlyoutMarginDIPs.store((int)margin);
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

        // Title — Segoe UI SemiBold 13pt, white
        HFONT fTitle = CreateFontW(
            -MulDiv(13, logPx, 72), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT fPrev = (HFONT)SelectObject(hdc, fTitle);
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
        int dpi = GetDpiForWindow(hwnd);
        // Flyout matches widget width, right-aligned to the widget's right edge.
        int marginPx  = MulDiv(g_FlyoutMarginDIPs.load(), dpi, 96);
        int widgetWPx = MulDiv(g_Settings.panelWidth,     dpi, 96);
        int flyoutH   = MulDiv(kFlyoutHDIPs,              dpi, 96);
        int x = tr.right - marginPx - widgetWPx;
        int y = tr.top   - flyoutH  - MulDiv(4, dpi, 96);
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

    g_FlyoutHwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_LAYERED,
        kClass, nullptr, WS_POPUP,
        0, 0, 300, 380,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

    if (g_FlyoutHwnd)
        SetLayeredWindowAttributes(g_FlyoutHwnd, 0,
            g_Settings.flyoutTransparent ? 235 : 255, LWA_ALPHA);

    SetEvent((HANDLE)readyEvent);  // signal that HWND is ready (or creation failed)

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    g_FlyoutHwnd = nullptr;
    return 0;
}

// ---------- Widget construction ----------
static Grid BuildWidget() {
    Grid root;
    root.Name(kWidgetRootName);
    root.Width((double)g_Settings.panelWidth);
    root.HorizontalAlignment(HorizontalAlignment::Right);
    root.VerticalAlignment(VerticalAlignment::Stretch);
    // Background (Acrylic) is applied on first call to ApplyStateToWidget().
    root.CornerRadius(CornerRadiusHelper::FromUniformRadius(8.0));
    Canvas::SetZIndex(root, 2);
    // Span all columns so right-alignment is relative to full taskbar width.
    Grid::SetColumnSpan(root, 9999);
    Grid::SetRowSpan(root, 9999);
    // Margin.Right is set dynamically via UpdateWidgetMargin() once tray width is known.

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

    // Session count chip (collapsed by default)
    TextBlock sessionCount;
    sessionCount.Name(kSessionCountName);
    sessionCount.Text(L"");
    sessionCount.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
    sessionCount.FontSize((double)g_Settings.fontSize);
    sessionCount.VerticalAlignment(VerticalAlignment::Center);
    sessionCount.Margin(ThicknessHelper::FromLengths(0, 0, 6, 0));
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
    Grid::SetColumn(sessionCount, 1);
    layout.Children().Append(sessionCount);

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
    skipBack.Content(box_value(hstring{L"«"})); // «
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
    playPause.Content(box_value(hstring{L"▶︎"})); // ▶ text-only rendering
    playPause.Background(MakeBrush(0x00, 0, 0, 0));
    playPause.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
    playPause.BorderThickness(ThicknessHelper::FromUniformLength(0));
    playPause.Padding(ThicknessHelper::FromLengths(4, 2, 4, 2));
    playPause.Margin(ThicknessHelper::FromLengths(2, 0, 2, 0));
    playPause.Width(30.0); // fixed width prevents layout shift when toggling ▶/⏸
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
    skipFwd.Content(box_value(hstring{L"»"})); // »
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
            if (g_FlyoutHwnd) PostMessageW(g_FlyoutHwnd, WM_FLYOUT_SHOW, 0, 0);
        }));
    root.PointerExited(PointerEventHandler(
        [](IInspectable const&, PointerRoutedEventArgs const&) {
            if (g_FlyoutHwnd) PostMessageW(g_FlyoutHwnd, WM_FLYOUT_HIDE_DELAYED, 0, 0);
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
    if (playBtn)  playBtn.Content(box_value(hstring{isPlaying ? L"\u23F8\uFE0E" : L"\u25B6\uFE0E"}));

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
            if (g_FlyoutHwnd) PostMessageW(g_FlyoutHwnd, WM_FLYOUT_UPDATE, 0, 0);
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
                                    if (g_FlyoutHwnd) PostMessageW(g_FlyoutHwnd, WM_FLYOUT_UPDATE, 0, 0);
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

    // Subscribe to tray resize to keep margin accurate when icon count changes.
    if (tray) {
        g_TrayResizeToken = tray.SizeChanged(
            [](IInspectable const&, SizeChangedEventArgs const&) {
                UpdateWidgetMargin();
            });
    }

    UpdateWidgetMargin();  // set initial Margin.Right = trayWidth + gap
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

    // Stop marquee and revoke SizeChanged token before removing the widget.
    StopMarquee();
    if (g_TitleSizeChangedToken.value) {
        if (auto tb = FindByName<TextBlock>(widget, kTitleName))
            tb.SizeChanged(g_TitleSizeChangedToken);
        g_TitleSizeChangedToken = {};
    }

    try {
        auto weakGrid = make_weak(rootGrid);
        rootGrid.Dispatcher().RunAsync(
            Windows::UI::Core::CoreDispatcherPriority::Normal,
            [weakGrid]() {
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
                if (g_FlyoutHwnd) PostMessageW(g_FlyoutHwnd, WM_FLYOUT_HIDE_NOW, 0, 0);
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
    {
        std::lock_guard<std::mutex> lk(g_MediaMutex);
        if (g_Unloading.load() || idx < 0 || idx >= g_MediaStateCount) co_return;
        session = g_MediaStates[idx].session;
    }
    if (!session) co_return;

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
        }

        // Fetch thumbnail stream reference (null for Libby and apps with no art)
        try {
            if (props) newThumbRef = props.Thumbnail();
        } WH_CATCH(L"UpdateOneSessionAsync/Thumbnail")
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
    std::thread([]() {
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
    }).detach();
}

// Polls for Taskbar.View.dll (or ExplorerExtensions.dll on older builds) every
// 100 ms for up to 60 s, then installs the UpdateVisualStates hook. Using
// GetModuleHandleW polling instead of a LoadLibraryExW hook avoids the
// cold-start trampoline race: on boot Explorer loads dozens of DLLs on multiple
// threads simultaneously, and patching LoadLibraryExW while another thread is
// executing it causes an unhandled hardware fault that kills Explorer.
static void PollForTaskbarViewDll() {
    std::thread([]() {
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
    }).detach();
}

// ---------- Mod entry points ----------
BOOL Wh_ModInit() {
    LoadSettings();

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

    // Shut down the flyout window and its thread before any other teardown.
    if (g_FlyoutHwnd) PostMessageW(g_FlyoutHwnd, WM_FLYOUT_QUIT, 0, 0);
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
    if (g_FlyoutHwnd)
        PostMessageW(g_FlyoutHwnd, WM_FLYOUT_SETTINGS, 0, 0);
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
        widget.Dispatcher().RunAsync(
            Windows::UI::Core::CoreDispatcherPriority::Normal,
            [weak, w, h, fs]() {
                auto g = weak.get();
                if (!g) return;
                g.Width((double)w);
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
