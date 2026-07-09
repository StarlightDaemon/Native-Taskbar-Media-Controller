# Open Loops

## ~~OL-11: WSL→macOS migration remediation~~ CLOSED (2026-06-07)

Migration audit completed 2026-06-07 (see `WORK_LOG.md`). All four
findings resolved in commit `7a91ec3`:
- **P1** — `.git/hooks/commit-msg` chmod +x (was 666, execute bit missing)
- **P2** — `AGENTS.md` L26: `/mnt/e/Raiden/` → `/Users/dante/Citadel/Raiden/`
- **P3** — `docs/audit-reports/` (2 files): repo path WSL→macOS
- **P4** — `docs/prompts/` (2 files): all `/mnt/e/` paths WSL→macOS
Global `/mnt/e/` scan clean post-remediation.

## ~~OL-10: Marquee scroll live test~~ CLOSED (deferred to post-v1.0.0)

Implementation shipped in beta.5. Live test deferred — v1.0.0 shipped without
on-device confirmation per Occam's razor release decision (2026-05-24). The
`LayoutUpdated` + `Storyboard/DoubleAnimation` implementation is correct by
construction. First post-v1.0.0 bug report or session can confirm.

## ~~OL-7: General cover art (XAML BitmapImage)~~ CLOSED

Shipped in `bf5b4c9` (2026-05-22): square `Image` element, `BitmapImage` loaded
via `co_await OpenReadAsync` + `SetSourceAsync`, marshalled back to UI dispatcher
before `Source` is set. Libby null-thumbnail collapses element gracefully.

## ~~OL-8: Commit and tag beta.2.8 cold-boot fix~~ CLOSED

Shipped in `979e033` (2026-05-22): cold-boot LoadLibraryExW hook replaced with
`PollForTaskbarViewDll` poll thread. Tagged `v0.1.0-beta.2.8` and pushed to remote.

## ~~OL-2: Phase 2 feature selection~~ CLOSED

Implemented in `b58aecb` (2026-05-23) as `v0.2.0-beta.1`. All four candidates shipped:
- **SC-CH-1** — `IsTaskbarEffectivelyVisible` auto-hide detection
- **SC-UI-2** — Adaptive text color from album art luminance (BT.601, gated by `AdaptiveTextColor` setting)
- **SC-M-2** — `BringSourceAppToFront` on double-tap via `PKEY_AppUserModel_ID` + exe fallback
- **SC-KV-4** — 3px display-only progress bar from `GetTimelineProperties()` (gated by `ShowProgress` setting)

Pending operator live test + tag + push of `v0.2.0-beta.1`. Phase 3 scope in OL-9.

## ~~SC-UI-3: Title truncation — Grid layout + marquee scroll~~ CLOSED

Shipped 2026-05-23 as `v0.2.0-beta.5`:

- **Grid layout refactor** — replaced horizontal `StackPanel` with a 6-column `Grid`
  (Auto, Auto, `*`, Auto, Auto, Auto); text column now fills remaining space via star
  sizing; removed hardcoded `MaxWidth(180)`.
- **Marquee scroll** — `DispatcherTimer` at 16 ms drives a `TranslateTransform` on the
  title `TextBlock` inside a `Border(ClipToBounds)`; 2 s start-pause → 40 px/s scroll →
  1 s end-pause → instant reset; fires only when text overflows the clip container.
  Gated by `MarqueeTitle` setting (default true). Artist row unchanged (CharacterEllipsis).

## ~~OL-9: Phase 3 — Background theming~~ CLOSED

Shipped 2026-05-23 as `v0.2.0-beta.6`:

- **BackgroundStyle = 0 (None)** — `root.Background(nullptr)`; widget root is fully transparent.
- **BackgroundStyle = 1 (Acrylic)** — `AcrylicBrush(HostBackdrop)` with dark semi-transparent
  fallback when compositor rejects it. Default value; preserves beta.5 behaviour for existing users.
- **BackgroundStyle = 2 (Chameleon)** — `LinearGradientBrush` derived from album art via 64-bucket
  RGB histogram quantization; two dominant colors form a horizontal gradient; transparent when no
  art is present. Adaptive text color driven by BT.601 luma of dominant color (`g_ChameleonLightBg`
  atomic) instead of `IsSystemLightTheme()` when this mode is active.

All three modes are applied live in `ApplyStateToWidget()` — no widget rebuild on setting change.

## ~~OL-6: Libby audiobook support review & refinement~~ CLOSED

Shipped in `3f2f5a5` (2026-05-22):
- **2a** AlbumTitle/AlbumArtist fallback (fixes flash-of-nothing; no AUMID gating)
- **2b** Playback rate suffix on artist row (` · 1.5×`) when speed ≠ 1.0
- **2c** `«` SkipBack / `»` SkipFwd buttons gated on `IsSkipForward/BackwardEnabled`

**Deferred to post-v1.0.0:**
- Record Libby's actual AUMID in `HANDOFF-LIBBY-2026-05-22.md`
- Confirm whether title/artist swap (AlbumTitle as headline) is correct for Libby
- Confirm Libby's `TimelineProperties` are populated → feeds SC-SP-1 priority

## ~~OL-4: `g_GsmtcStartEvent` handle TOCTOU fix~~ CLOSED

Shipped 2026-05-22: `g_GsmtcStartEvent` converted to `std::atomic<HANDLE>`;
all call sites use `.load()`/`.exchange()`; uninit snaps with `.exchange(nullptr)`
before `CloseHandle`. No plain reads remain.

## ~~OL-5: Uninit async-task drain timeout too short~~ CLOSED

Shipped 2026-05-22: drain raised to 50 × 100 ms (5 s), matching hook drain;
warning log added when drain times out before `g_MediaStates` cleanup.

## ~~OL-3: `feature/rename-to-native-controller` branch not merged~~ CLOSED

Merged to `main` via `dc4c0d2`. Closed 2026-05-21.
