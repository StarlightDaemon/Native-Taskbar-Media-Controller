# Open Loops

## ~~OL-7: General cover art (XAML BitmapImage)~~ CLOSED

Shipped in `bf5b4c9` (2026-05-22): square `Image` element, `BitmapImage` loaded
via `co_await OpenReadAsync` + `SetSourceAsync`, marshalled back to UI dispatcher
before `Source` is set. Libby null-thumbnail collapses element gracefully.

## ~~OL-8: Commit and tag beta.2.8 cold-boot fix~~ CLOSED

Shipped in `979e033` (2026-05-22): cold-boot LoadLibraryExW hook replaced with
`PollForTaskbarViewDll` poll thread. Tagged `v0.1.0-beta.2.8` and pushed to remote.

## OL-2: Phase 2 feature selection

Fork synthesis is complete (see `fork-reports/synthesis-2026-05-19.md`). Phase 2 feature scope has not been decided. Operator must select which synthesis candidates to implement next.

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
