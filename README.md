# Native Taskbar Media Controller

> A [Windhawk](https://windhawk.net) mod that injects a native media controller directly into the Windows 11 taskbar.

**Status: v1.4.9**

Unlike overlay-based taskbar media mods, this one inserts the widget as a real child element of the taskbar's own XAML tree (`Grid#RootGrid` under `Taskbar.TaskbarFrame`). There is no separate window, no `SetLayeredWindowAttributes`, no GDI painting loop. The widget inherits correct z-ordering, auto-hide handling, and DPI scaling from the taskbar itself.

## Features

- **Now playing** — title and artist from any GSMTC-compatible source: Spotify, YouTube Music, Windows Media Player, browsers, audiobook apps, and anything else that registers a media session
- **Playback controls** — play/pause toggle, skip-next, and skip-back; skip buttons are always shown but dimmed when not supported by the source
- **Hover flyout** — hovering over the widget shows an expanded panel with full-width album art, title, and artist; follows the system dark/light theme
- **Multi-session** — when multiple media apps are active simultaneously, a session count chip appears in the top-right corner; tap it to cycle through sessions
- **Audiobook mode** — sessions longer than one hour (or with chapter keywords) are treated as audiobooks: skip buttons navigate chapters and playback speed is shown next to the title

<details>
<summary>More features</summary>

Also includes: scrolling marquee title, inline album art, track progress bar with position/duration timestamp, smooth progress interpolation between SMTC ticks, text crossfade on track changes, widget fade animations, acrylic frosted-glass background, adaptive text color, fullscreen auto-hide, live repositioning as tray icons change, double-click to focus the source app, and middle-click to stop the session.

</details>

## Compatibility

- **Spotify, Apple Music, Tidal, VLC, Windows Media Player** and most native apps work out of the box
- **Browsers** (Chrome, Edge, Brave, Opera, Vivaldi, Arc, Thorium): one SMTC session per browser process; all tabs share it; timeline data is not available from browser sessions
- **Firefox**: media info is fully supported; timeline data is not available (Mozilla Bugzilla 1689538)
- **Libby, Audiobookshelf, Audible Cloud Player**: audiobook sessions are fully supported via audiobook mode
- **Audible native app**: discontinued January 2022 — does not register SMTC sessions

## Requirements

- Windows 11 (22H2 or later recommended)
- [Windhawk](https://windhawk.net) mod loader

## Installation

1. Install [Windhawk](https://windhawk.net) if you haven't already
2. In the Windhawk app, click **Find mods** and search for **Native Taskbar Media Controller**, or load the `.wh.cpp` source file directly via **Develop** → **Load mod from file**
3. Click **Install** (or compile in dev mode) — the panel appears without an Explorer restart

<details>
<summary>Settings</summary>

| Setting | Default | Description |
|---|---|---|
| Widget width (px) | 300 | Width of the media panel |
| Widget height (px) | 40 | Height — should be slightly less than your taskbar height |
| Font size | 11 | Font size for title and artist text |
| Gap from tray (px) | 8 | Horizontal gap between the widget and the system tray |
| Hide when fullscreen | true | Collapses the panel when a fullscreen or presentation state is detected |
| Show track progress bar and timestamp | true | Enables the slim progress bar and position/duration display; hidden automatically when the source doesn't expose timeline data |
| Adaptive text color | true | Follows the Windows light/dark theme for text and button colors |
| Scrolling title | true | Enables the marquee ticker for titles that overflow the widget width |
| Flyout transparency | false | On: hover flyout is slightly transparent (92% opaque). Off: flyout uses a solid background matching the system theme |

</details>

## Roadmap

For the full analysis of feature candidates across 11 community forks, see [`fork-reports/synthesis-2026-05-19.md`](fork-reports/synthesis-2026-05-19.md).

### Under consideration

- Synchronized LRC lyrics (SC-HT-1)
- FFT audio visualizer (SC-GR-1)

<details>
<summary>Acknowledgements</summary>

This mod was built by studying the following community forks of the original [Taskbar Music Lounge](https://github.com/ramensoftware/windhawk-mods/blob/53d96781b3215f0a082908a2539cafe178e8895a/mods/taskbar-music-lounge.wh.cpp) by Hashah2311 (author's GitHub account no longer exists; link points to the preserved source in the Windhawk community mods repo). Design ideas and implementation patterns from each fork informed the architecture and roadmap.

| Author | Fork |
|---|---|
| Hashah2311 | Original Taskbar Music Lounge (baseline) |
| Messij | taskbar-music-lounge-multiple |
| memeri121 | taskbar-spotify-widget |
| Chaython & Hashah2311 | taskbar-music-lounge (joint fork) |
| Uiisland | taskbar-music-lounge-fork-v4-merged-adaptive |
| kevinoe | taskbar-media-widget |
| Cinabutts | taskbar-music-lounge-pro |
| 0xjio | taskbar-media-beacon |
| HibritTofas | taskbar-media-bar |
| GR0UD | taskbar-media-player |

The XAML injection scaffold pattern is adapted from [bbmaster123's tb-video-injector](https://github.com/bbmaster123/FWFU).

</details>

## License

MIT — see [LICENSE](LICENSE).
