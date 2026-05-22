# Hardening Handoff — 2026-05-22

## Context

This handoff was produced after a security review of the current `main` branch
(`0155fa3`). The review found no HIGH-confidence security vulnerabilities. One
real defect (confidence 5/10) was identified and one robustness concern was
noted. Both are targeted at clearing before a `v1.0.0` release.

The only file to edit is:
`native-taskbar-media-controller.wh.cpp`

---

## Item 1 — `g_GsmtcStartEvent` Handle TOCTOU (Priority: High)

### Problem

`g_GsmtcStartEvent` is a plain `HANDLE` global (line 173). It is read and used
(via `SetEvent`) from at least three call sites:

- `InjectWidgetInto` — line 484 and line 531–532 (runs on UI/dispatcher thread)
- `TriggerInitialScan` — line 926–927 (runs on a detached `std::thread`)

In `Wh_ModUninit` (line 1007), the handle is closed and the pointer nulled:

```cpp
if (g_GsmtcStartEvent) { CloseHandle(g_GsmtcStartEvent); g_GsmtcStartEvent = nullptr; }
```

There is a window between the null-check (`if (g_GsmtcStartEvent)`) and the
`SetEvent` call in `InjectWidgetInto`/`TriggerInitialScan` where uninit can
close the handle. Calling `SetEvent` on a closed HANDLE is undefined behaviour;
on Windows, the handle value may be recycled by a different kernel object.

The `g_Unloading` guard in `ScheduleScanAsync` (line 539) is upstream of
`InjectWidgetInto` but is not atomic with the `SetEvent` call site.

### Fix

Replace the plain `HANDLE` with `std::atomic<HANDLE>`. Use
`InterlockedExchangePointer` (or the atomic equivalent) to snap the pointer to
`nullptr` *before* closing in uninit, then close the saved copy. At call sites,
load the handle into a local before use.

Concrete steps:

1. Change the declaration (line 173):
   ```cpp
   // Before
   static HANDLE g_GsmtcStartEvent = nullptr;
   // After
   static std::atomic<HANDLE> g_GsmtcStartEvent{ nullptr };
   ```

2. Update `Wh_ModInit` (line 979) — `CreateEventW` result assigned directly:
   ```cpp
   g_GsmtcStartEvent.store(CreateEventW(nullptr, TRUE, FALSE, nullptr));
   ```

3. Update every `SetEvent(g_GsmtcStartEvent)` call site (lines 484, 532, 927,
   993) to load into a local first:
   ```cpp
   if (HANDLE ev = g_GsmtcStartEvent.load()) SetEvent(ev);
   ```

4. Update `WaitForSingleObject(g_GsmtcStartEvent, INFINITE)` in
   `GsmtcThreadFunc` (line 728) similarly:
   ```cpp
   if (HANDLE ev = g_GsmtcStartEvent.load()) {
       DWORD wr = WaitForSingleObject(ev, INFINITE);
       Wh_Log(L"[gsmtc] thread: start event wait returned %lu", wr);
   }
   ```

5. Update the log line (line 981) that prints `g_GsmtcStartEvent` directly:
   ```cpp
   Wh_Log(L"[init] GSMTC thread: handle=%p tid=%lu event=%p",
          g_GsmtcThread, g_GsmtcThreadId, g_GsmtcStartEvent.load());
   ```

6. In `Wh_ModUninit` (lines 993, 1007), snap-and-close:
   ```cpp
   // Signal before snapping so the thread wakes.
   if (HANDLE ev = g_GsmtcStartEvent.load()) SetEvent(ev);
   // ... wait for g_GsmtcThread ...
   // Then close:
   if (HANDLE ev = g_GsmtcStartEvent.exchange(nullptr)) CloseHandle(ev);
   ```

Success criterion: no plain reads of `g_GsmtcStartEvent` remain; every access
goes through `.load()` or `.exchange()`.

---

## Item 2 — Uninit Drain Timeouts (Priority: Low)

### Problem

Two spin-drains in `Wh_ModUninit` use fixed iteration counts:

```cpp
// Hook counter drain (line ~994 in current file)
for (int i = 0; i < 50 && g_HookCallCounter.load() > 0; ++i) Sleep(100);  // 5 s max

// Async task drain (added in 0155fa3)
for (int i = 0; i < 20 && g_AsyncTasks.load() > 0; ++i) Sleep(100);       // 2 s max
```

If `g_AsyncTasks` is still non-zero after 2 s (e.g., a `co_await` is stuck
waiting on a slow media session), the process continues teardown while
coroutine frames are still live. This can cause a use-after-free on the
`g_MediaStates` array if a coroutine resumes after the array is cleaned up.

### Fix

Before the `g_MediaStates` cleanup block, log a warning if the drain timed out:

```cpp
if (g_AsyncTasks.load() > 0) {
    Wh_Log(L"[uninit] WARNING: async task drain timed out (%d tasks remaining)",
           g_AsyncTasks.load());
}
```

Additionally, extend the async drain to match the hook drain (50 × 100 ms = 5 s)
so both have the same headroom. The coroutines hold a reference to
`g_MediaStates` entries which are cleaned up under `g_MediaMutex` immediately
after the drain — if a coroutine resumes after cleanup it will find
`g_Unloading = true` and `co_return` early, but only if the post-drain cleanup
hasn't already freed/zeroed the array. Raising the limit gives more margin.

---

## What is NOT in this handoff

- Phase 2 feature candidates (SC-SP-1, SC-HT-1, SC-GR-1, etc.) — those are
  tracked in GOALS.md and OPEN_LOOPS.md (OL-2).
- The security review found no HIGH-confidence vulnerabilities. This handoff
  covers only the two defects noted above.

## Verification

After applying both fixes:
1. Build the mod with Windhawk's build system and confirm it loads into
   explorer.exe without crash.
2. Enable verbose logging (`Wh_Log`) and confirm:
   - GSMTC thread start event signals correctly.
   - Mod unloads cleanly (no handle-closed log errors).
3. Commit with message referencing this handoff date.
