// ==WindhawkMod==
// @id              native-taskbar-media-controller
// @name            Native Taskbar Media Controller
// @description     Native XAML-injected media controller in the Windows 11 taskbar — shows now-playing info with playback controls.
// @version         0.1.0-beta.1
// @author          StarlightDaemon
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lole32 -loleaut32 -lruntimeobject -luser32 -lwindowsapp -lversion -lshell32 -DWINVER=0x0A00 -Wl,--undefined=__imp_FindWindowW -Wl,--undefined=__imp_FindWindowExW -Wl,--undefined=__imp_PostMessageW -Wl,--undefined=__imp_GetClientRect
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Native Taskbar Media Controller

Injects a media controller natively into the Windows 11 taskbar XAML tree — no overlay window,
no separate process. The widget lives as a real child of the taskbar's own UI, giving it correct
z-ordering, auto-hide support, and DPI handling automatically.

## Features

- **Now playing** — title and artist from any GSMTC-compatible app (Spotify, YouTube Music,
  Windows Media Player, browsers, etc.)
- **Playback controls** — play/pause toggle and skip-next buttons
- **Multi-session** — a session count chip appears when multiple media apps are active; tap it
  to cycle between sessions
- **Fullscreen auto-hide** — panel collapses automatically when a fullscreen app is detected
- **Configurable** — panel width/height, font size, tray gap, fullscreen behavior

## Settings

| Setting | Default | Notes |
|---|---|---|
| Widget width (px) | 300 | |
| Widget height (px) | 40 | |
| Font size | 11 | |
| Gap from tray (px) | 8 | Extra spacing between the widget and the system tray |
| Hide when fullscreen | true | |

## Requirements

- Windows 11 (22H2 or later)
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
*/
// ==/WindhawkModSettings==

#include <windhawk_api.h>
#include <windhawk_utils.h>

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>


// winbase.h defines GetCurrentTime() as a macro wrapping GetTickCount().
// winrt XAML headers declare a virtual GetCurrentTime(int64_t*) method.
// Undefine the macro before pulling in WinRT to avoid the collision.
#undef GetCurrentTime

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
#include <winrt/Windows.UI.Xaml.Media.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Media::Control;
using namespace Windows::UI;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Automation;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;

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
} g_Settings;

static void LoadSettings() {
    g_Settings.panelWidth   = Wh_GetIntSetting(L"PanelWidth");
    g_Settings.panelHeight  = Wh_GetIntSetting(L"PanelHeight");
    g_Settings.fontSize     = Wh_GetIntSetting(L"FontSize");
    g_Settings.offsetX      = Wh_GetIntSetting(L"OffsetX");
    g_Settings.hideFullscreen = Wh_GetIntSetting(L"HideFullscreen") != 0;
    if (g_Settings.panelWidth   <= 0) g_Settings.panelWidth = 300;
    if (g_Settings.panelHeight  <= 0) g_Settings.panelHeight = 40;
    if (g_Settings.fontSize     <= 0) g_Settings.fontSize = 11;
    if (g_Settings.offsetX      <  0) g_Settings.offsetX = 8;
}

// ---------- GSMTC multi-session state ----------
static constexpr int MAX_SESSIONS = 10;

struct MediaState {
    std::wstring title;
    std::wstring artist;
    std::wstring sessionId;    // AUMID (SourceAppUserModelId)
    bool isPlaying = false;
    double playbackRate = 1.0;  // listen speed; 1.0 = normal
    bool canSkipForward  = false; // SMTC IsSkipForwardEnabled
    bool canSkipBackward = false; // SMTC IsSkipBackwardEnabled
    int64_t positionMs = 0;
    int64_t durationMs = 0;
    GlobalSystemMediaTransportControlsSession session{ nullptr };
    event_token propsChangedToken{};
    event_token playbackChangedToken{};
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
constexpr std::wstring_view kNextName       = L"NowPlayingNext";
constexpr std::wstring_view kSkipBackName   = L"NowPlayingSkipBack";
constexpr std::wstring_view kSkipFwdName    = L"NowPlayingSkipFwd";
constexpr std::wstring_view kSessionCountName = L"NowPlayingSessionCount";
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
static HANDLE g_PollThread = nullptr;
static HANDLE g_PollStop = nullptr;
static std::atomic<HWND> g_hTaskbarWnd{ nullptr };

// ---------- Helpers ----------
static FrameworkElement GetFrameworkElementFromNative(void* pThis) {
    // pThis points to a WinRT implements<> object. Offset +3 (24 bytes) is where
    // the IInspectable vtable shim lives inside the object for TaskListButton.
    // The address of that slot IS the COM interface pointer — do not dereference.
    try {
        void* ifacePtr = (void**)pThis + 3;
        IInspectable insp{ nullptr };
        copy_from_abi(insp, ifacePtr);
        auto fe = insp.try_as<FrameworkElement>();
        return fe;
    } catch (...) {
        return nullptr;
    }
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

static SolidColorBrush MakeBrush(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return SolidColorBrush(ColorHelper::FromArgb(a, r, g, b));
}

// Forward
static void RefreshWidgetUI();
static void UpdateWidgetMargin();

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
    Wh_Log(L"[pos] trayWidth=%.0f gap=%.0f => Margin.Right=%.0f", trayWidth, gap, margin);
    widget.Margin(ThicknessHelper::FromLengths(0, 0, margin, 0));
}

// ---------- Widget construction ----------
static Grid BuildWidget() {
    Grid root;
    root.Name(kWidgetRootName);
    root.Width((double)g_Settings.panelWidth);
    root.Height((double)g_Settings.panelHeight);
    root.HorizontalAlignment(HorizontalAlignment::Right);
    root.VerticalAlignment(VerticalAlignment::Center);
    root.Background(MakeBrush(0xCC, 0x1A, 0x1A, 0x1A));
    root.CornerRadius(CornerRadiusHelper::FromUniformRadius(8.0));
    Canvas::SetZIndex(root, 2);
    // Span all columns so right-alignment is relative to full taskbar width.
    Grid::SetColumnSpan(root, 9999);
    Grid::SetRowSpan(root, 9999);
    // Margin.Right is set dynamically via UpdateWidgetMargin() once tray width is known.

    StackPanel layout;
    layout.Orientation(Orientation::Horizontal);
    layout.VerticalAlignment(VerticalAlignment::Center);
    layout.Margin(ThicknessHelper::FromLengths(8.0, 0.0, 8.0, 0.0));

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
    layout.Children().Append(sessionCount);

    // Title / artist column
    StackPanel textCol;
    textCol.Orientation(Orientation::Vertical);
    textCol.VerticalAlignment(VerticalAlignment::Center);
    textCol.MaxWidth(180.0);

    TextBlock title;
    title.Name(kTitleName);
    title.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
    title.FontSize((double)g_Settings.fontSize);
    title.TextTrimming(TextTrimming::CharacterEllipsis);
    title.TextWrapping(TextWrapping::NoWrap);
    title.MaxLines(1);

    TextBlock artist;
    artist.Name(kArtistName);
    artist.Foreground(MakeBrush(0xB3, 0xFF, 0xFF, 0xFF));
    artist.FontSize((double)g_Settings.fontSize);
    artist.TextTrimming(TextTrimming::CharacterEllipsis);
    artist.TextWrapping(TextWrapping::NoWrap);
    artist.MaxLines(1);

    textCol.Children().Append(title);
    textCol.Children().Append(artist);
    layout.Children().Append(textCol);

    // 2c: Skip Backward — 30-second rewind; shown only when session enables it
    Button skipBack;
    skipBack.Name(kSkipBackName);
    skipBack.Content(box_value(hstring{L"«"})); // «
    skipBack.Background(MakeBrush(0x00, 0, 0, 0));
    skipBack.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
    skipBack.BorderThickness(ThicknessHelper::FromUniformLength(0));
    skipBack.Padding(ThicknessHelper::FromLengths(6, 2, 6, 2));
    skipBack.Margin(ThicknessHelper::FromLengths(8, 0, 2, 0));
    skipBack.VerticalAlignment(VerticalAlignment::Center);
    skipBack.Visibility(Visibility::Collapsed);
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
    layout.Children().Append(skipBack);

    // Play/Pause
    Button playPause;
    playPause.Name(kPlayPauseName);
    playPause.Content(box_value(hstring{L"▶"})); // ▶
    playPause.Background(MakeBrush(0x00, 0, 0, 0));
    playPause.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
    playPause.BorderThickness(ThicknessHelper::FromUniformLength(0));
    playPause.Padding(ThicknessHelper::FromLengths(6, 2, 6, 2));
    playPause.Margin(ThicknessHelper::FromLengths(2, 0, 2, 0));
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
    layout.Children().Append(playPause);

    // Next track (hidden when SkipForward is available, to avoid redundancy)
    Button next;
    next.Name(kNextName);
    next.Content(box_value(hstring{L"⏭"})); // ⏭
    next.Background(MakeBrush(0x00, 0, 0, 0));
    next.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
    next.BorderThickness(ThicknessHelper::FromUniformLength(0));
    next.Padding(ThicknessHelper::FromLengths(6, 2, 6, 2));
    next.Margin(ThicknessHelper::FromLengths(2, 0, 0, 0));
    next.VerticalAlignment(VerticalAlignment::Center);
    AutomationProperties::SetName(next, L"Next track");
    next.Click(RoutedEventHandler(
        [](IInspectable const&, RoutedEventArgs const&) {
            GlobalSystemMediaTransportControlsSession s{ nullptr };
            {
                std::lock_guard<std::mutex> g(g_MediaMutex);
                if (g_ActiveSessionIndex >= 0 && g_ActiveSessionIndex < g_MediaStateCount) {
                    s = g_MediaStates[g_ActiveSessionIndex].session;
                }
            }
            if (s) {
                try { s.TrySkipNextAsync(); } WH_CATCH(L"Next/SkipNext")
            }
        }));
    layout.Children().Append(next);

    // 2c: Skip Forward — 30-second advance; shown only when session enables it
    Button skipFwd;
    skipFwd.Name(kSkipFwdName);
    skipFwd.Content(box_value(hstring{L"»"})); // »
    skipFwd.Background(MakeBrush(0x00, 0, 0, 0));
    skipFwd.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
    skipFwd.BorderThickness(ThicknessHelper::FromUniformLength(0));
    skipFwd.Padding(ThicknessHelper::FromLengths(6, 2, 6, 2));
    skipFwd.Margin(ThicknessHelper::FromLengths(2, 0, 0, 0));
    skipFwd.VerticalAlignment(VerticalAlignment::Center);
    skipFwd.Visibility(Visibility::Collapsed);
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
    layout.Children().Append(skipFwd);

    root.Children().Append(layout);
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

static void ApplyStateToWidget(Grid widget) {
    if (!widget) return;

    int count = 0;
    int activeIdx = 0;
    bool hasMedia = false;
    std::wstring title, artist;
    bool isPlaying = false;
    double playbackRate = 1.0;
    bool canSkipForward = false, canSkipBackward = false;
    {
        std::lock_guard<std::mutex> g(g_MediaMutex);
        count = g_MediaStateCount;
        activeIdx = g_ActiveSessionIndex;
        if (activeIdx >= 0 && activeIdx < count) {
            auto& m = g_MediaStates[activeIdx];
            title          = m.title;
            artist         = m.artist;
            isPlaying      = m.isPlaying;
            playbackRate   = m.playbackRate;
            canSkipForward  = m.canSkipForward;
            canSkipBackward = m.canSkipBackward;
            hasMedia = !title.empty();
        }
    }

    // 2b: Build artist display string; append rate suffix when speed != 1.0×
    std::wstring artistDisplay = artist;
    if (std::fabs(playbackRate - 1.0) > 0.01) {
        wchar_t rateBuf[16];
        swprintf(rateBuf, 16, L"%.4g\u00D7", playbackRate); // e.g. "1.5×"
        artistDisplay += artistDisplay.empty() ? rateBuf
                                               : (std::wstring(L" \u00B7 ") + rateBuf);
    }

    auto titleTb    = FindByName<TextBlock>(widget, kTitleName);
    auto artistTb   = FindByName<TextBlock>(widget, kArtistName);
    auto playBtn    = FindByName<Button>(widget, kPlayPauseName);
    auto sessTb     = FindByName<TextBlock>(widget, kSessionCountName);
    auto nextBtn    = FindByName<Button>(widget, kNextName);
    auto skipFwdBtn = FindByName<Button>(widget, kSkipFwdName);
    auto skipBackBtn= FindByName<Button>(widget, kSkipBackName);

    if (titleTb)  titleTb.Text(hasMedia ? title         : L"");
    if (artistTb) artistTb.Text(hasMedia ? artistDisplay : L"");
    if (playBtn)  playBtn.Content(box_value(hstring{isPlaying ? L"\u23F8" : L"\u25B6"}));
    if (sessTb) {
        if (count > 1) {
            sessTb.Text(std::to_wstring(count));
            sessTb.Visibility(Visibility::Visible);
        } else {
            sessTb.Text(L"");
            sessTb.Visibility(Visibility::Collapsed);
        }
    }

    // 2c: Show skip buttons when enabled; hide Next when SkipForward is present
    if (skipBackBtn)
        skipBackBtn.Visibility(canSkipBackward ? Visibility::Visible : Visibility::Collapsed);
    if (skipFwdBtn)
        skipFwdBtn.Visibility(canSkipForward  ? Visibility::Visible : Visibility::Collapsed);
    if (nextBtn)
        nextBtn.Visibility(canSkipForward ? Visibility::Collapsed : Visibility::Visible);

    widget.Visibility(hasMedia ? Visibility::Visible : Visibility::Collapsed);
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
        Wh_Log(L"[inject] widget already present — refreshing state");
        {
            std::lock_guard<std::mutex> g(g_WidgetMutex);
            g_WidgetRoot = make_weak(existing);
            g_RootGrid   = make_weak(rootGrid);
            ApplyStateToWidget(existing);
        }
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
        g_hTaskbarWnd.store(FindWindowW(L"Shell_TrayWnd", nullptr));
        // Detach old tray resize subscription if present.
        if (tray && g_TrayResizeToken.value) {
            try { tray.SizeChanged(g_TrayResizeToken); } catch (...) {}
            g_TrayResizeToken = {};
        }
    }

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

    if (HANDLE ev = g_GsmtcStartEvent.load()) {
        bool signaled = SetEvent(ev);
        Wh_Log(L"[inject] signaled GSMTC start event (SetEvent=%d)", (int)signaled);
    }
}

static void ScheduleScanAsync(FrameworkElement startNode) {
    if (!startNode) return;
    if (g_Unloading.load()) return;
    bool expected = false;
    if (!g_ScanPending.compare_exchange_strong(expected, true)) return;

    Wh_Log(L"[inject] ScheduleScanAsync started on dispatcher");

    auto weak = make_weak(startNode);
    try {
        startNode.Dispatcher().RunAsync(
            Windows::UI::Core::CoreDispatcherPriority::Low,
            [weak]() {
                g_ScanPending = false;
                if (g_Unloading.load()) return;
                auto node = weak.get();
                if (!node) {
                    Wh_Log(L"[inject] Weak reference to startNode lost");
                    return;
                }
                try {
                    auto frame = WalkUpToTaskbarFrame(node);
                    if (!frame) {
                        Wh_Log(L"[inject] WalkUpToTaskbarFrame returned null");
                        return;
                    }
                    auto rootGrid = FindRootGrid(frame);
                    if (!rootGrid) {
                        Wh_Log(L"[inject] FindRootGrid returned null");
                        return;
                    }
                    Wh_Log(L"[inject] Found rootGrid, injecting...");
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
        auto weakGrid = make_weak(rootGrid);
        rootGrid.Dispatcher().RunAsync(
            Windows::UI::Core::CoreDispatcherPriority::Normal,
            [weakGrid]() {
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
        }
        auto pb = session.GetPlaybackInfo();
        if (pb) {
            playing = (pb.PlaybackStatus()
                == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing);
            // 2b: Playback rate (IReference<double> — null when not supported)
            if (auto rateRef = pb.PlaybackRate()) playbackRate = rateRef.Value();
            // 2c: Skip forward / backward capability flags
            if (auto ctrls = pb.Controls()) {
                canSkipForward  = ctrls.IsSkipForwardEnabled();
                canSkipBackward = ctrls.IsSkipBackwardEnabled();
            }
        }
    } WH_CATCH(L"UpdateOneSessionAsync")

    {
        std::lock_guard<std::mutex> lk(g_MediaMutex);
        if (g_Unloading.load() || idx >= g_MediaStateCount) co_return;
        if (g_MediaStates[idx].session != session) co_return;

        auto& m = g_MediaStates[idx];
        if (m.title == title && m.artist == artist && m.isPlaying == playing
            && std::fabs(m.playbackRate - playbackRate) < 0.001
            && m.canSkipForward == canSkipForward
            && m.canSkipBackward == canSkipBackward) co_return;
        m.title          = title;
        m.artist         = artist;
        m.isPlaying      = playing;
        m.playbackRate   = playbackRate;
        m.canSkipForward  = canSkipForward;
        m.canSkipBackward = canSkipBackward;
    }
    RefreshWidgetUI();
}

static void DetachSessionLocked(int idx) {
    if (idx < 0 || idx >= g_MediaStateCount) return;
    auto& m = g_MediaStates[idx];
    try {
        if (m.session) {
            if (m.propsChangedToken.value)    m.session.MediaPropertiesChanged(m.propsChangedToken);
            if (m.playbackChangedToken.value) m.session.PlaybackInfoChanged(m.playbackChangedToken);
        }
    } catch (...) {}
    m = MediaState{};
}

static void DoEnumerateAndRefresh() {
    Wh_Log(L"[gsmtc] DoEnumerateAndRefresh: entered");
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

    {
        std::lock_guard<std::mutex> lk(g_MediaMutex);
        if (g_Unloading.load()) return;
        for (int i = 0; i < g_MediaStateCount; ++i) DetachSessionLocked(i);
        g_MediaStateCount = 0;

        int playingIdx = -1;
        for (auto const& s : sessions) {
            if (g_MediaStateCount >= MAX_SESSIONS) break;
            auto& m = g_MediaStates[g_MediaStateCount];
            m.session = s;
            m.sessionId = WH_TRY_OR(std::wstring(s.SourceAppUserModelId().c_str()), std::wstring(L""));
            try {
                m.propsChangedToken = s.MediaPropertiesChanged(
                    [idx = g_MediaStateCount](auto&&, auto&&) { UpdateOneSessionAsync(idx); });
            } WH_CATCH(L"DoEnum/MediaPropertiesChanged")
            try {
                m.playbackChangedToken = s.PlaybackInfoChanged(
                    [idx = g_MediaStateCount](auto&&, auto&&) { UpdateOneSessionAsync(idx); });
            } WH_CATCH(L"DoEnum/PlaybackInfoChanged")
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
        Wh_Log(L"[gsmtc] enumerated %d session(s), active=%d", g_MediaStateCount, g_ActiveSessionIndex);
    }

    int n;
    { std::lock_guard<std::mutex> lk(g_MediaMutex); n = g_MediaStateCount; }
    for (int i = 0; i < n; ++i) UpdateOneSessionAsync(i);
    RefreshWidgetUI();
}

static DWORD WINAPI GsmtcThreadFunc(LPVOID) {
    Wh_Log(L"[gsmtc] thread: started, tid=%lu", GetCurrentThreadId());
    try {
        init_apartment(apartment_type::single_threaded);
        Wh_Log(L"[gsmtc] thread: STA init OK");
    } WH_CATCH(L"GsmtcThread/STA")

    Wh_Log(L"[gsmtc] thread: waiting for start event (g_GsmtcStartEvent=%p)", g_GsmtcStartEvent.load());
    if (HANDLE ev = g_GsmtcStartEvent.load()) {
        DWORD wr = WaitForSingleObject(ev, INFINITE);
        Wh_Log(L"[gsmtc] thread: start event wait returned %lu (OBJECT_0=0)", wr);
    }

    if (g_Unloading.load()) {
        Wh_Log(L"[gsmtc] thread: unloading flag set after wait — exiting");
        return 0;
    }

    Wh_Log(L"[gsmtc] thread: calling RequestAsync");
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
    Wh_Log(L"[gsmtc] thread: request created, status=%d", (int)req.Status());

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

        Wh_Log(L"[gsmtc] Completed callback: status=%d tid=%lu", (int)status, GetCurrentThreadId());

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
        Wh_Log(L"[gsmtc] manager set OK, registering SessionsChanged");

        try {
            g_SessionsChangedToken = g_SessionManager.SessionsChanged(
                [](auto&&, auto&&) {
                    Wh_Log(L"[gsmtc] SessionsChanged fired");
                    DoEnumerateAndRefresh();
                });
        } WH_CATCH(L"GsmtcThread/SessionsChanged")

        DoEnumerateAndRefresh();
    });

    Wh_Log(L"[gsmtc] thread: Completed callback registered, entering STA message pump");
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_QUIT) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    Wh_Log(L"[gsmtc] thread: STA message pump exited");
    return 0;
}

// ---------- Fullscreen polling ----------
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
        bool hide = (state == QUNS_PRESENTATION_MODE)
                 || (state == QUNS_RUNNING_D3D_FULL_SCREEN
                     && IsForegroundWindowFullscreen(hTaskbarMon))
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
                            w.Visibility(Visibility::Collapsed);
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
    g_HookCallCounter++;
    TaskListButton_UpdateVisualStates_Original(pThis);
    if (!g_Unloading.load()) {
        auto elem = GetFrameworkElementFromNative(pThis);
        if (elem) ScheduleScanAsync(elem);
    }
    g_HookCallCounter--;
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

using LoadLibraryExW_t = decltype(&LoadLibraryExW);
static LoadLibraryExW_t LoadLibraryExW_Original = nullptr;

// Post WM_SIZE to Shell_TrayWnd (and secondary bars) to trigger UpdateVisualStates
// without waiting for user interaction. Called after hooks are applied.
static void TriggerInitialScan() {
    std::thread([]() {
        Sleep(200);
        HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
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
        // Signal the GSMTC thread in case the hook didn't fire yet.
        if (HANDLE ev = g_GsmtcStartEvent.load()) {
            SetEvent(ev);
            Wh_Log(L"[init] signaled GSMTC start event from TriggerInitialScan");
        }
    }).detach();
}

static HMODULE WINAPI LoadLibraryExW_Hook(LPCWSTR name, HANDLE hFile, DWORD flags) {
    HMODULE module = LoadLibraryExW_Original(name, hFile, flags);
    if (module && !g_TaskbarViewDllLoaded.load() && name) {
        if (wcsstr(name, L"Taskbar.View.dll") || wcsstr(name, L"ExplorerExtensions.dll")) {
            bool already = g_TaskbarViewDllLoaded.exchange(true);
            if (!already) {
                HookTaskbarViewDllSymbols(module);
                Wh_ApplyHookOperations();
                TriggerInitialScan();
            }
        }
    }
    return module;
}

// ---------- Mod entry points ----------
BOOL Wh_ModInit() {
    Wh_Log(L"native-taskbar-media-controller: init (tid=%lu)", GetCurrentThreadId());
    LoadSettings();

    try {
        init_apartment(apartment_type::multi_threaded);
        Wh_Log(L"[init] Wh_ModInit apartment init OK");
    } catch (winrt::hresult_error const& e) {
        Wh_Log(L"[init] Wh_ModInit apartment init threw hr=0x%08X (thread already initialized — OK)", (unsigned)e.code());
    } catch (...) {
        Wh_Log(L"[init] Wh_ModInit apartment init threw unknown");
    }

    HMODULE taskbarView = GetModuleHandleW(L"Taskbar.View.dll");
    if (!taskbarView) taskbarView = GetModuleHandleW(L"ExplorerExtensions.dll");
    Wh_Log(L"[init] Taskbar.View module: %p", taskbarView);

    if (taskbarView) {
        g_TaskbarViewDllLoaded = true;
        if (!HookTaskbarViewDllSymbols(taskbarView)) return FALSE;
    } else {
        Wh_SetFunctionHook((void*)LoadLibraryExW,
                           (void*)LoadLibraryExW_Hook,
                           (void**)&LoadLibraryExW_Original);
    }

    g_PollStop   = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_PollThread = CreateThread(nullptr, 0, FullscreenPollThread, nullptr, 0, nullptr);
    Wh_Log(L"[init] poll thread: handle=%p", g_PollThread);

    g_GsmtcStartEvent.store(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    g_GsmtcThread     = CreateThread(nullptr, 0, GsmtcThreadFunc, nullptr, 0, &g_GsmtcThreadId);
    Wh_Log(L"[init] GSMTC thread: handle=%p tid=%lu event=%p", g_GsmtcThread, g_GsmtcThreadId, g_GsmtcStartEvent.load());

    TriggerInitialScan();
    Wh_Log(L"[init] Wh_ModInit complete");
    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L"native-taskbar-media-controller: uninit");
    g_Unloading = true;

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

    {
        std::lock_guard<std::mutex> g(g_MediaMutex);
        if (g_PendingRequest) {
            try { g_PendingRequest.Cancel(); } catch (...) {}
        }
        try {
            if (g_SessionManager && g_SessionsChangedToken.value) {
                g_SessionManager.SessionsChanged(g_SessionsChangedToken);
                g_SessionsChangedToken = {};
            }
        } catch (...) {}
        for (int i = 0; i < g_MediaStateCount; ++i) DetachSessionLocked(i);
        g_MediaStateCount = 0;
        g_SessionManager = nullptr;
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
                g.Height((double)h);
                if (auto t = FindByName<TextBlock>(g, kTitleName))  t.FontSize(fs);
                if (auto a = FindByName<TextBlock>(g, kArtistName)) a.FontSize(fs);
                if (auto s = FindByName<TextBlock>(g, kSessionCountName)) s.FontSize(fs);
                ApplyStateToWidget(g);
                UpdateWidgetMargin();  // must run on UI thread
            });
    } catch (...) {}
}
