# Native Taskbar Media Controller

> A [Windhawk](https://windhawk.net) mod that injects a native media controller directly into the Windows 11 taskbar.

**Status: beta — v0.1.0-beta.1**

Unlike overlay-based taskbar media mods, this one inserts the widget as a real child element of the taskbar's own XAML tree (`Grid#RootGrid` under `Taskbar.TaskbarFrame`). There is no separate window, no `SetLayeredWindowAttributes`, no GDI painting loop. The widget inherits correct z-ordering, auto-hide handling, and DPI scaling from the taskbar itself.

## Features

- **Now playing** — title and artist from any GSMTC-compatible source: Spotify, YouTube Music, Windows Media Player, browsers, and anything else that registers a media session
- **Playback controls** — play/pause toggle and skip-next
- **Multi-session** — when multiple media apps are active simultaneously, a session count chip appears; tap it to cycle through sessions
- **Fullscreen auto-hide** — the panel collapses automatically when a fullscreen app is running (configurable)
- **Live repositioning** — the panel tracks the system tray width in real time, so adding or removing tray icons keeps it correctly positioned

## Requirements

- Windows 11 (22H2 or later recommended)
- [Windhawk](https://windhawk.net) mod loader

## Installation

1. Install [Windhawk](https://windhawk.net) if you haven't already
2. In the Windhawk app, click **Find mods** and search for **Native Taskbar Media Controller**, or load the `.wh.cpp` source file directly via **Develop** → **Load mod from file**
3. Click **Install** (or compile in dev mode) — the panel appears without an Explorer restart

## Settings

| Setting | Default | Description |
|---|---|---|
| Widget width (px) | 300 | Width of the media panel |
| Widget height (px) | 40 | Height — should be slightly less than your taskbar height |
| Font size | 11 | Font size for title and artist text |
| Gap from tray (px) | 200 | Right margin. **Increase this if the panel overlaps the clock or tray icons.** Typical range: 180–380 depending on your icon count |
| Hide when fullscreen | true | Collapses the panel when a fullscreen or presentation state is detected |

## Roadmap

Phase 2 features are under consideration. Priority order is approximate.

### Planned

- **Auto-hide taskbar compatibility** — correct detection when the taskbar is set to auto-hide
- **Monitor-coverage fullscreen detection** — more precise than the current notification-state poll
- **Click to focus** — bring the source media app to the foreground on click

### Under consideration

- Seek bar with drag support
- Track progress bar (display-only)
- Album art display
- Blurred / chameleon background derived from album art
- Synchronized LRC lyrics
- Shuffle / repeat status display

For the full analysis of 39 feature candidates across 11 community forks, see [`fork-reports/synthesis-2026-05-19.md`](fork-reports/synthesis-2026-05-19.md).

## Acknowledgements

This mod was built by studying the following community forks of the original [Taskbar Music Lounge](https://github.com/Hashah2311/taskbar-music-lounge) by Hashah2311. Design ideas and implementation patterns from each fork informed the architecture and roadmap.

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

## License

MIT — see [LICENSE](LICENSE).
