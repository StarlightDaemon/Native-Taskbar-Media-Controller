# Open Loops

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

## OL-4: `g_GsmtcStartEvent` handle TOCTOU fix (Priority: High)

Plain `HANDLE` global read from multiple threads without atomicity guarantee.
`SetEvent` can be called on a closed handle if uninit races the injection path.
Fix: convert to `std::atomic<HANDLE>`, use `.load()`/`.exchange()` at all
call sites. Full spec in `HANDOFF-HARDENING-2026-05-22.md` — Item 1.

## OL-5: Uninit async-task drain timeout too short (Priority: Low)

`g_AsyncTasks` drain is capped at 2 s (20 × 100 ms); hook drain is 5 s.
If a coroutine resumes after the drain, it accesses `g_MediaStates` which may
be cleaned up. Fix: extend drain to 50 iterations; add warning log on timeout.
Full spec in `HANDOFF-HARDENING-2026-05-22.md` — Item 2.

## ~~OL-3: `feature/rename-to-native-controller` branch not merged~~ CLOSED

Merged to `main` via `dc4c0d2`. Closed 2026-05-21.
