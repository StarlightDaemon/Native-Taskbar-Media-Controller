# Open Loops

<!-- Statuses: open · deferred · held: <who> · no-action · accepted · closed. Each active item: ID, status, gate owner, next step or restart condition, evidence. IDs never change or get reused. Entries below stay in their original order. -->

## Active

- **OL-12 · open · gate: none (ordinary work)** · Trim `CURRENT_STATE.md` to the current release (v1.5.0); move per-release history to `WORK_LOG.md` or drop it where git tags carry it; no superseded "Latest release tag" line may survive. Details in the OL-12 entry below.
- All other entries are closed.

## OL-12: CURRENT_STATE.md release-note accretion (audit P3 residue)

Filed 2026-07-26 per framework decision `Raiden:D-0058` — audit findings rated
Medium or higher must become loops or be explicitly dispositioned. This is the
**unresolved remainder** of P3 ("state and doc sync", F1a/F1c/F1d/F1e/F3a/F1f)
from the 2026-06-24 audit, which `last-audit.md` recorded as "in progress this
session" and which was then tracked nowhere for a month.

- **Status:** open
- **Gate:** none — ordinary work
- Verified 2026-07-26, the version-sync half of P3 **is** done: `README.md:5`,
  `CURRENT_STATE.md:7`, and `GOALS.md:15` all state v1.5.0, and the repo's
  latest tag is in fact `v1.5.0`.
- What remains is the drift the 2026-06-24 audit named in its own dominant
  theme: `CURRENT_STATE.md` has accreted into a changelog. It is ~10 KB of
  stacked per-release sections, and the historical ones carry claims that read
  as current — most clearly line 34, "Latest release tag: `v1.4.8` (2026-05-25)
  — superseded by v1.4.9", sitting two releases behind the same file's own
  header, and referencing a v1.4.9 that no other surface mentions.
- **Why it matters:** `CURRENT_STATE.md` is defined as current-facts-only and
  overwritten, not appended (state schema v2 / D-0051). A file that stacks
  superseded release sections has a second, contradictory copy of a fact whose
  home is elsewhere — `WORK_LOG.md` for dated history, git tags for releases.
- **Success condition:** `CURRENT_STATE.md` describes only the current release;
  per-release history moves to `WORK_LOG.md` or is dropped where git tags
  already carry it; no superseded "Latest release tag" line survives.

## ~~OL-13: Scope ledger — register or retire SC-HT-5 / SC-HT-3 (audit P4)~~ closed (2026-07-26)

Filed and closed 2026-07-26 per `Raiden:D-0058`. P4 from the 2026-06-24 audit
(F3b/F3c) was outstanding outside the loop system.

**Disposition: resolved — verified, not assumed.** Both scope items are now
registered as explicitly deferred, in two places: `README.md:77-78` ("deferred;
no implementation plan yet" / "deferred; low priority") and `GOALS.md:39-40`.
The second half of P4, "populate DECISIONS.md", is likewise done —
`.raiden/state/DECISIONS.md` carries eight entries (D-1 through D-8), including
the two permanent refusals D-4 and D-5. The work had been completed; only the
tracking was missing.

## ~~OL-14: Git and ignore hygiene (audit P5)~~ closed (2026-07-26)

Filed and closed 2026-07-26 per `Raiden:D-0058`. P5 from the 2026-06-24 audit
(F4/F5) was outstanding outside the loop system.

**Disposition: resolved — verified, not assumed.** Both legs check out on disk:
`.claude/` is present in the repo `.gitignore` (line 21), and the stale
`feature/rename-to-native-controller` branch is gone — `git branch -a` shows
only `main` and its remote tracking refs. (That branch also has its own closed
entry, OL-3, below.) Again the work was done and only the tracking was missing.

## ~~OL-11: WSL→macOS migration remediation~~ closed (2026-06-07)

Migration audit completed 2026-06-07 (see `WORK_LOG.md`). All four
findings resolved in commit `b09a4a4`:
- **P1** — `.git/hooks/commit-msg` chmod +x (was 666, execute bit missing)
- **P2** — `AGENTS.md` L26: `/mnt/e/Raiden/` → `E:\Citadel/Raiden/`
- **P3** — `docs/audit-reports/` (2 files): repo path WSL→macOS
- **P4** — `docs/prompts/` (2 files): all `/mnt/e/` paths WSL→macOS
Global `/mnt/e/` scan clean post-remediation.

## ~~OL-10: Marquee scroll live test~~ closed (deferred to post-v1.0.0)

Implementation shipped in beta.5. Live test deferred — v1.0.0 shipped without
on-device confirmation per Occam's razor release decision (2026-05-24). The
`LayoutUpdated` + `Storyboard/DoubleAnimation` implementation is correct by
construction. First post-v1.0.0 bug report or session can confirm.

## ~~OL-7: General cover art (XAML BitmapImage)~~ closed

Shipped in `29bc1e5` (2026-05-22): square `Image` element, `BitmapImage` loaded
via `co_await OpenReadAsync` + `SetSourceAsync`, marshalled back to UI dispatcher
before `Source` is set. Libby null-thumbnail collapses element gracefully.

## ~~OL-8: Commit and tag beta.2.8 cold-boot fix~~ closed

Shipped in `e6bdae7` (2026-05-22): cold-boot LoadLibraryExW hook replaced with
`PollForTaskbarViewDll` poll thread. Tagged `v0.1.0-beta.2.8` and pushed to remote.

## ~~OL-2: Phase 2 feature selection~~ closed

Implemented in `a502184` (2026-05-23) as `v0.2.0-beta.1`. All four candidates shipped:
- **SC-CH-1** — `IsTaskbarEffectivelyVisible` auto-hide detection
- **SC-UI-2** — Adaptive text color from album art luminance (BT.601, gated by `AdaptiveTextColor` setting)
- **SC-M-2** — `BringSourceAppToFront` on double-tap via `PKEY_AppUserModel_ID` + exe fallback
- **SC-KV-4** — 3px display-only progress bar from `GetTimelineProperties()` (gated by `ShowProgress` setting)

Pending operator live test + tag + push of `v0.2.0-beta.1`. Phase 3 scope in OL-9.

## ~~SC-UI-3: Title truncation — Grid layout + marquee scroll~~ closed

Shipped 2026-05-23 as `v0.2.0-beta.5`:

- **Grid layout refactor** — replaced horizontal `StackPanel` with a 6-column `Grid`
  (Auto, Auto, `*`, Auto, Auto, Auto); text column now fills remaining space via star
  sizing; removed hardcoded `MaxWidth(180)`.
- **Marquee scroll** — `DispatcherTimer` at 16 ms drives a `TranslateTransform` on the
  title `TextBlock` inside a `Border(ClipToBounds)`; 2 s start-pause → 40 px/s scroll →
  1 s end-pause → instant reset; fires only when text overflows the clip container.
  Gated by `MarqueeTitle` setting (default true). Artist row unchanged (CharacterEllipsis).

## ~~OL-9: Phase 3 — Background theming~~ closed

Shipped 2026-05-23 as `v0.2.0-beta.6`:

- **BackgroundStyle = 0 (None)** — `root.Background(nullptr)`; widget root is fully transparent.
- **BackgroundStyle = 1 (Acrylic)** — `AcrylicBrush(HostBackdrop)` with dark semi-transparent
  fallback when compositor rejects it. Default value; preserves beta.5 behaviour for existing users.
- **BackgroundStyle = 2 (Chameleon)** — `LinearGradientBrush` derived from album art via 64-bucket
  RGB histogram quantization; two dominant colors form a horizontal gradient; transparent when no
  art is present. Adaptive text color driven by BT.601 luma of dominant color (`g_ChameleonLightBg`
  atomic) instead of `IsSystemLightTheme()` when this mode is active.

All three modes are applied live in `ApplyStateToWidget()` — no widget rebuild on setting change.

## ~~OL-6: Libby audiobook support review & refinement~~ closed

Shipped in `fb42605` (2026-05-22):
- **2a** AlbumTitle/AlbumArtist fallback (fixes flash-of-nothing; no AUMID gating)
- **2b** Playback rate suffix on artist row (` · 1.5×`) when speed ≠ 1.0
- **2c** `«` SkipBack / `»` SkipFwd buttons gated on `IsSkipForward/BackwardEnabled`

**Deferred to post-v1.0.0:**
- Record Libby's actual AUMID in `HANDOFF-LIBBY-2026-05-22.md`
- Confirm whether title/artist swap (AlbumTitle as headline) is correct for Libby
- Confirm Libby's `TimelineProperties` are populated → feeds SC-SP-1 priority

## ~~OL-4: `g_GsmtcStartEvent` handle TOCTOU fix~~ closed

Shipped 2026-05-22: `g_GsmtcStartEvent` converted to `std::atomic<HANDLE>`;
all call sites use `.load()`/`.exchange()`; uninit snaps with `.exchange(nullptr)`
before `CloseHandle`. No plain reads remain.

## ~~OL-5: Uninit async-task drain timeout too short~~ closed

Shipped 2026-05-22: drain raised to 50 × 100 ms (5 s), matching hook drain;
warning log added when drain times out before `g_MediaStates` cleanup.

## ~~OL-3: `feature/rename-to-native-controller` branch not merged~~ closed

Merged to `main` via `1d0bc57`. Closed 2026-05-21.

### LOOP-SWEEP-20260831-001 — native-taskbar-media-controller registry/journal Edict Version mismatch

- Status: closed — resolved 2026-10-17; verified journal attribution repair
- Gate: operator
- Success Condition: registry/instances.md Edict Version and the distribution log's latest journalled version agree for native-taskbar-media-controller (next fleet sweep no longer flags registry_journal_mismatch).
- Check: registry_journal_mismatch:native-taskbar-media-controller
- Resolution: the central attribution review verified installed metadata and
  baseline consistency and repaired narrative journal matching. The current
  registry and journal agree; no Edict was distributed and no installed version
  was changed. This closes the reporting finding, not product acceptance.
- Evidence: Raiden-ops:LOOP-0030 and the 2026-10-17 generated fleet check.

