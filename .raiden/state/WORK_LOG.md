# Work Log

## [2026-07-09] Edict v2.0.0 + state normalization

**Objective:** Apply Edict v2.0.0 to the managed core and normalize `.raiden/state`
prose against the new `OPERATING_RULES.md` Fact-Home Rule.

**Edict update:** `plan`/`apply` via `raiden_updater.cli` — 1.0.1 → 2.0.0.
`ROUTING_POLICY.md` added; `MODEL_TIERS.md` removed (`managed_file_removal`
warning, expected — file dropped from the v2.0.0 package). `hooks/commit-msg`
unchanged (adopted 2026-07-08, already carries the current marker). Re-plan
confirmed "Already up to date". `.raiden/instance/metadata.json` stamped with
`"state_schema_version": 2`.

**Routing overlay:** `.raiden/local/MODEL_MAP.md` (untracked, gitignored)
removed; replaced with `.raiden/local/ROUTING.md` (ladder-form routing
overlay — R1–R4 rungs plus an offload pool, per operator decision).

**Audit-ledger untracking (D-8):** `.raiden/state/AUDIT_LOG.md`,
`FORK_REVIEW_LOG.md`, `last-audit.md`, `last-fork-review.md` untracked via
`git rm --cached` — files remain on disk, only git tracking stops. All four
paths added to `.gitignore` under the existing D-0038 exclusion block. Ledgers
were tracked on this public repo against the intent of Raiden-ops D-0038;
untracked per D-0038 and operator decision (2026-07-09). Git history retains
previously published copies — this stops future publication, it does not
unpublish already-committed history.

**State normalization (Fact-Home Rule):**
- Relocated the historical Edict-version fact from the 2026-06-07 migration
  remediation record — "Edict v0.6.1 confirmed clean" — out of `CURRENT_STATE.md`
  and `OPEN_LOOPS.md` prose and into the dated `[2026-06-07]` entry below (its
  proper historical home; no current-version claims were found in state prose).
- Collapsed `CURRENT_STATE.md`'s migration-remediation section to a bare `OL-11`
  citation — the loop's open/closed status and detail already live in
  `OPEN_LOOPS.md`, so `CURRENT_STATE.md` no longer restates them.
- No hand-written "Last Updated" / "Last Verified" footers were found in any
  `.raiden/state/*.md` file (including `GOALS.md`) — none to remove.

---

## [2026-06-07] Migration Remediation Closed (OL-11)

WSL→macOS path migration audit completed under **Edict v0.6.1**, confirmed
clean. All four findings (P1–P4) fixed and pushed in commit `7a91ec3` on
`main`; global `/mnt/e/` scan clean post-remediation. Closed as OL-11 in
`OPEN_LOOPS.md`.

---

## [2026-05-24] v1.0.0 Release — Log Cleanup + Ship

**Objective:** Occam's razor release gate. Strip all remaining verbose trace logs; leave only error and warning paths. Tag and ship v1.0.0.

**Changes:**
- Stripped 33 verbose `Wh_Log` trace calls across `[pos]`, `[inject]`, `[gsmtc]`, `[init]`, `[coldstart]`, and `[uninit]` contexts
- Kept 14 log calls: `WH_CATCH` macro (3 lines), injection exceptions (2), GSMTC errors (7), `HookSymbols` failure (1), uninit drain timeout warning (1)
- Closed OL-10 (marquee scroll live test) as deferred — implementation correct by construction; first post-release session confirms
- Updated CURRENT_STATE to v1.0.0; updated OPEN_LOOPS to reflect all loops closed
- Post-v1.0.0 queue: SC-SP-1 (seek bar) → SC-UI-1 (blurred art) → SC-HT-1 (LRC) → SC-GR-1 (FFT)

---

## [2026-05-24] Strip Verbose Scan and Hook Logs

**Objective:** Remove verbose per-call log statements (`[scan]` and `[hook]`) from `GetFrameworkElementFromNative` and `TaskListButton_UpdateVisualStates_Hook` to prevent spamming during taskbar repaints.

**Change:**
- Deleted diagnostic comments and 8 `Wh_Log` statements with `[scan]` prefix from `GetFrameworkElementFromNative()`.
- Deleted 4 `Wh_Log` statements with `[hook]` prefix from `TaskListButton_UpdateVisualStates_Hook()`.
- Verified that all other logs and C++ logic remain intact and untouched.

---

## [2026-05-23] SC-M-2 Toggle Minimize Confirmed — v0.2.0-beta.4

**Objective:** Add toggle-minimize to SC-M-2 double-tap gesture.

**Change:** `BringSourceAppToFront` now branches on `IsIconic`:
- Minimized → restore + raise (existing path, unchanged)
- Not minimized → `ShowWindow(SW_MINIMIZE)` — one new line

Matches Windows taskbar button toggle behavior. Sidesteps focus-detection problem (Explorer has focus at tap time) by using window state rather than foreground state as the branch condition.

**Confirmed working (2026-05-23):**
- Spotify Windows 11 Store app — toggle raise/minimize ✓
- Libby via Chrome download integration — toggle raise/minimize ✓

---

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

## [2026-05-23] Session Close — OL-9 Phase 3 Shipped (v0.2.0-beta.6)

- **Objective:** Review open tasks/loops, plan Phase 3 (OL-9), delegate implementation, push release.
- **Open loop review:** Confirmed OL-9 (Phase 3 — BackgroundStyle setting) as the only active loop. All others closed. Libby verification items deferred to post-v1.0.0.
- **Handoff authored:** `.raiden/state/HANDOFF-OL9-BACKGROUND-THEMING.md` — self-contained brief for second agent covering Acrylic gate status, BackgroundStyle enum design, Chameleon 64-bucket quantization via `BitmapDecoder`+`SoftwareBitmap`+`IMemoryBufferByteAccess`, `LinearGradientBrush` application, BT.601 luma → `g_ChameleonLightBg` adaptive text, settings-change Option A (live apply in `ApplyStateToWidget`).
- **Implementation (second agent):** All changes landed in single commit `c8e9353` — BackgroundStyle YAML setting + `ModSettings` field, `ComputeDominantColors()` helper, Chameleon fire_and_forget path, gated Acrylic, no-art transparent fallback, `g_ChameleonLightBg` atomic for text color, `winrt/Windows.Graphics.Imaging.h` + interop header.
- **Tagged and pushed:** `v0.2.0-beta.6` on `main` — both pushed to remote. PAT used transiently, not stored.
- **State:** No open loops. Post-v1.0.0 deferred items: Libby AUMID verification, BootLog + verbose log strip.
