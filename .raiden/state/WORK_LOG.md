# Work Log

## [2026-05-23] SC-M-2 Confirmed Working — v0.2.0-beta.3

**Objective:** Diagnose and fix SC-M-2 double-tap focus — had been failing in every live test since beta.1.

**Root causes identified and fixed:**

1. **`AttachThreadInput` wrong operands** — previous code: `AttachThreadInput(fgTid, tgtTid, TRUE)`. This gave the *target app's thread* Explorer's foreground lock, which is useless — `SetForegroundWindow` is called from the XAML dispatcher thread, a third unattached thread. Fixed to `AttachThreadInput(ourTid, fgTid, TRUE)` where `ourTid = GetCurrentThreadId()`. Added `AllowSetForegroundWindow(ASFW_ANY)` as belt-and-suspenders (pattern from hibrittofas/messij forks).

2. **`ExtractExeHint` broken for Store AUMIDs** — for `SpotifyAB.SpotifyMusic_zpdnekdrzrea0!Spotify`, old code took the pre-`!` package name and tried to strip `.exe` — finding none, returned `spotifyab.spotifymusic_zpdnekdrzrea0`. No process matches this. Fixed: if pre-bang portion doesn't end in `.exe` (Store AUMID), use the post-bang AppId (`Spotify`) as the exe hint → matches `Spotify.exe` correctly.

**Also added:** BootLog diagnostics in `BringSourceAppToFront` — logs the AUMID searched, derived hint, and whether a window was found/raised.

**Live test result:** Confirmed working with Spotify Windows Store app.

**Phase 2 status:** All four features confirmed — SC-CH-1 ✓, SC-UI-2 ✓, SC-M-2 ✓, SC-KV-4 ✓. Ready to tag + push `v0.2.0-beta.3`.

---

## [2026-05-23] Session Close — v0.2.0-beta.2 (Phase 2 visual polish, SC-M-2 pending)

**Objective:** Validate Phase 2 features live; fix visual issues reported in screenshot.

**Changes shipped in v0.2.0-beta.2:**
- **AcrylicBrush background** (Phase 3 gate pulled forward): replaced `MakeBrush(0xCC,...)` with `AcrylicBrush` (`HostBackdrop`, tint `0x1A` dark, opacity 0.6) in `BuildWidget()`; fallback to solid on compositor reject. Confirmed working in Explorer's injected XAML tree.
- **Progress bar Rectangle replacement**: replaced `ProgressBar` (system accent color, uncontrollable template) with a `Grid` track + `Shapes::Rectangle` fill. Track: 20% fg-color; fill: 80% fg-color; width driven by `positionMs/durationMs * ActualWidth`.
- **Full theme propagation (SC-UI-2 extension)**: buttons (`playBtn`, `nextBtn`, `skipFwdBtn`, `skipBackBtn`), session chip, progress track background, and progress fill all now flip via `fgHi` in `ApplyStateToWidget` — confirmed working on both dark and light Windows theme.
- **SC-M-2 fix attempt**: replaced bare `SetForegroundWindow` with `AttachThreadInput(fgTid, tgtTid, TRUE)` + `BringWindowToTop` + `SetForegroundWindow` + detach. Still not working — root cause unknown.

**Live test results:**
- SC-CH-1 (auto-hide): ✓ confirmed
- SC-UI-2 (system theme color): ✓ confirmed — text, buttons, progress all adapt
- SC-KV-4 (progress bar): ✓ confirmed
- SC-M-2 (double-tap focus): ✗ still failing after AttachThreadInput fix

**Open:** SC-M-2 root cause to be diagnosed next session before tag + push.

---

## [2026-05-22] MILESTONE — Cold-Boot Explorer Crash Resolved (v0.1.0-beta.2.8)

**Objective:** Eliminate 100% reproducible Explorer crash on cold system boot.

**Root cause (confirmed):** On true cold boot, Windhawk injects into Explorer before `Taskbar.View.dll` is loaded. The previous cold-start path in `Wh_ModInit` created three threads (`FullscreenPollThread`, `PollForTaskbarViewDll`, `TriggerInitialScan`) and called `init_apartment(multi_threaded)` during Explorer's hazardous early-boot window. Boot log analysis showed all threads logging "started" at the exact same millisecond, then Explorer dying within 100ms — before any thread's first `Sleep(100)` expired. No hooks were installed; the crash was caused by thread creation and COM initialization during early boot, not by hook or GSMTC code.

**Fix (beta.2.8):** Made the cold-start path in `Wh_ModInit` create exactly **one thread** (the poll thread) and return immediately — no `init_apartment`, no `FullscreenPollThread`, no `g_GsmtcStartEvent`, no `TriggerInitialScan`. All deferred initialization now runs from `PollForTaskbarViewDll` after `Taskbar.View.dll` is detected and `Wh_ApplyHookOperations` completes, at which point XAML is confirmed loaded and the boot window has passed.

**Investigation path (multiple sessions):**
- beta.2.6: Diagnosed via boot log; `init_apartment(single_threaded)` in `GsmtcThreadFunc` implicated → fixed by deferring GSMTC thread to after DLL detection (beta.2.7)
- beta.2.7: Boot log showed crash still occurring within 100ms of Wh_ModInit, with 3 threads sleeping and no hooks — ruling out hook code and GSMTC as cause
- beta.2.8: Reduced cold-start footprint to one thread; crash resolved — confirmed by operator

**Confirmed working:** Cold boot no longer crashes Explorer.

## [2026-05-18] Fork File Organization Session
- **Objective:** Organize all fork files into `/forks/{author}/{fork-name}/` and produce an inventory manifest.
- **Forks Organized:** 13 fork files found; consolidated into 11 unique organized fork locations.
- **Flagged Files:** 
  - Duplicate filename conflict in `mod.wh.cpp` for Messij and Hashah2311/Messij forks (flagged in MANIFEST.md).
- **Confirmation:** Confirmed that no fork content was analyzed or modified during this session.

## [2026-05-19] Edict and Fork Verification Session
- **Objective:** Verify 0.5.0 package update installation, edict presence and integrity, and fork review readiness.
- **0.5.0 Install Confirmation:** Confirmed installed (metadata.json and baseline.json updated to 0.5.0).
- **Edict Integrity:** Confirmed `FORK_REVIEW_PROTOCOL.md` and `WORKSPACE_AUDIT_PROTOCOL.md` are present and intact in `.raiden/writ/`.
- **Fork Inventory:** 11 unique forks (13 entries in manifest due to duplicate conflicts) verified in `/forks/MANIFEST.md`. Baseline identified at `/forks/og_Hashah2311_taskbar-music-lounge.wh/og_Hashah2311_taskbar-music-lounge.wh.cpp`.
- **Readiness:** Confirmed no loose top-level files in `/forks/` (except `MANIFEST.md`), `fork-reports/` is currently absent, and the Instance is fully ready for fork review invocation.

## [2026-05-22] Session Close — v0.1.0-beta.2 Tagged and Pushed
- **Objective:** Review repo state, commit pending handoffs, bump version, push.
- **Version bumped:** `0.1.0-beta.1` → `0.1.0-beta.2` in mod header and README.
- **Committed:** Three session handoff files (HANDOFF-COVERART, HANDOFF-HARDENING, HANDOFF-LIBBY) staged and committed alongside version bump (commit `41884f3`).
- **Tagged:** `v0.1.0-beta.2` annotated tag created and pushed to remote.
- **Branch:** `main` up to date on remote.
- **PAT policy:** PAT used transiently for push; not stored or committed.

## [2026-05-21] Phase 1 Fixes, GitHub Setup, README, and v0.1.0-beta.1 Release
- **Objective:** Review repo state, fix bugs, push to GitHub, add README, cut beta release.
- **Bugs fixed in `native-taskbar-media-controller.wh.cpp`:**
  - Stale log strings (`taskbar-media-widget: init/uninit` → correct name)
  - `Wh_ModSettingsChanged` was setting `Margin.Right` from raw `offsetX`, bypassing `UpdateWidgetMargin()` — fixed to call `UpdateWidgetMargin()` after resize
  - `OffsetX` YAML default was `8`; aligned to `200` to match struct default and implementation spec
- **GitHub remote configured:** `https://github.com/StarlightDaemon/Native-Taskbar-Media-Controller`
  - Merged remote LICENSE (unrelated history) into local `main`
  - Rebased `feature/rename-to-native-controller` onto updated `main`
  - Pushed both branches; remote URL sanitized (PAT stripped) after each push
- **README.md created:** Features, installation, settings table, roadmap, fork acknowledgements, license.
- **Mod header updated:** `@author` → StarlightDaemon, `@version` → `0.1.0-beta.1`, description and `WindhawkModReadme` block rewritten for public release.
- **Release cut:** `v0.1.0-beta.1` tagged on `main`, GitHub pre-release published.
- **Raiden state updated:** CURRENT_STATE, GOALS, OPEN_LOOPS all populated.
- **Branch status:** `feature/rename-to-native-controller` is unmerged into `main` locally — wait, actually it WAS merged into main via `--no-ff` merge commit `dc4c0d2`. Both branches are up to date on remote.
- **PAT policy:** Two PATs were used and revoked by operator within the same session. Token is never committed or stored; remote URL is sanitized after each use.
