# Current State

Current facts only. Dated history is in `WORK_LOG.md`; releases are git tags.

## Release

- **v1.5.0**, released 2026-06-24: tag `v1.5.0` at `c07fbf3`. The mod is listed in the Windhawk mod catalog.
- Source: the single file `native-taskbar-media-controller.wh.cpp` (D-6). `main` is the only branch.

## What the mod does (v1.5.0)

- Injects its widget into the taskbar's own XAML tree (`Grid#RootGrid` under `Taskbar.TaskbarFrame`); no overlay window (D-1).
- Shows title, artist and album art from any GSMTC media session. Tracks up to 10 sessions at once; a session-count chip cycles between them.
- Controls: play/pause, skip back and forward (dimmed when the source doesn't support them), double-click to bring the source app to the front, middle-click to close the session.
- Hover flyout: a Win32 popup on its own thread with full-width album art, title (font steps down from 13pt to 9pt to fit) and artist; follows the dark/light theme; optional transparency (D-2).
- Audiobook mode: sessions over an hour, or with chapter keywords, use the skip buttons for chapters and show the playback speed.
- Display: marquee title, progress bar with timestamps and smooth interpolation, text crossfade, widget fade, Acrylic background only (D-3), adaptive text colour, fullscreen and auto-hide-taskbar hiding.
- Position setting: Right, Left or Center, with `OffsetX`. Right mode follows the tray width live.
- Teardown: every event token is revoked, the `InitialScan` and `PollForDll` threads are joined, and XAML work stays on the UI thread (D-7).

## Known limits

- Browser sources: Chromium shares one media session per browser process; Firefox provides no timeline; Libby (a Chrome web app) provides no cover art and no playback position.

## Scope

- **Will not be implemented:** the interactive seek bar SC-SP-1 (D-4) and display-only mode SC-0X-1 (D-5).
- **Next candidates, in order:** SC-HT-1 LRC lyrics overlay (a natural second tier for the flyout); SC-GR-1 FFT audio visualizer (needs a process-compatibility audit first).
- **Deferred:** SC-HT-5 per-app volume and SC-HT-3 shuffle/repeat status.
- **Parked idea:** a Chrome extension plus native-messaging companion that passes richer Media Session data (such as Libby chapters) to the mod. Revisit only once the core mod is stable.

## Open items

None. See `OPEN_LOOPS.md`.
