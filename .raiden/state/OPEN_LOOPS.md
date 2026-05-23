# Open Loops

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

## OL-9: Phase 3 — Background theming

Decided 2026-05-23. Two theming options to be implemented under a single `BackgroundStyle` setting:

- **Acrylic** — `AcrylicBrush` on the XAML panel background; no art processing; needs compositor compatibility test inside Explorer's injected XAML tree first
- **Chameleon** — `LinearGradientBrush` derived from album art via 64-bucket color quantization (SC-HT-2); runs after art loads; adaptive text color (SC-UI-2) feeds from same palette

**Setting shape:** `BackgroundStyle` enum: `None` / `Acrylic` / `Chameleon`

**Prerequisite:** SC-UI-2 (adaptive text color) should ship in Phase 2 first — Phase 3 Chameleon path extends it.

**Gate:** Confirm `AcrylicBrush` composites correctly in Explorer's XAML tree before shipping Acrylic option. If compositor rejects it, remove that enum value; Chameleon is unaffected.

## ~~OL-6: Libby audiobook support review & refinement~~ CLOSED

Shipped in `3f2f5a5` (2026-05-22):
- **2a** AlbumTitle/AlbumArtist fallback (fixes flash-of-nothing; no AUMID gating)
- **2b** Playback rate suffix on artist row (` · 1.5×`) when speed ≠ 1.0
- **2c** `«` SkipBack / `»` SkipFwd buttons gated on `IsSkipForward/BackwardEnabled`

**Still open (operator action required after live Libby test):**
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
