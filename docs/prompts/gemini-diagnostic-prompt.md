# Gemini Diagnostic Prompt — Explorer Cold-Boot Crash

You are a senior Windows systems engineer reviewing a crash investigation. You must NOT produce any code edits. Your output must be analysis, hypotheses, and diagnostic recommendations only.

---

## Project context

This is a Windhawk mod (`native-taskbar-media-controller.wh.cpp`) that injects a native XAML widget into the Windows 11 taskbar. It runs inside `explorer.exe` on Windows 11 24H2 build 26200.8457. The mod hooks `winrt::Taskbar::implementation::TaskListButton::UpdateVisualStates` (resolved from symbol cache at offset 0x13E54 in `Taskbar.View.dll`) using Windhawk's `Wh_HookSymbols` API. The hook fires whenever a taskbar button updates its visual state; the hook uses `QueryInterface` to obtain the `FrameworkElement` from the button's `pThis` pointer, then walks the XAML tree to inject a media controller widget.

**The mod works perfectly** when enabled or disabled while Explorer is already running. The crash only occurs on **cold system boot** (fresh system restart).

---

## The crash pattern — 100% reproducible

From a persistent boot log (`C:\Users\Public\wh-media-boot.log`) written synchronously via `CreateFile`/`WriteFile`/`CloseHandle` at each step, every single session that starts with `Taskbar.View.dll NOT loaded` (cold-start path) ends in an Explorer crash. Every session where `Taskbar.View.dll is already loaded` (direct path) works correctly.

The log entries that distinguish the two paths:
```
Cold-start: "Wh_ModInit: Taskbar.View.dll not loaded — starting poll thread"
Direct:     "Wh_ModInit: Taskbar.View.dll already loaded — direct path"
```

**No exceptions.** Every cold-start crashes. Every direct-path works.

---

## What the cold-start path does (current build)

`Wh_ModInit` runs. `GetModuleHandleW("Taskbar.View.dll")` returns null. Instead of hooking `LoadLibraryExW` (that was removed after suspecting a trampoline-write race on the boot DLL-load storm), the mod now:

1. Detaches `PollForTaskbarViewDll()` — polls `GetModuleHandleW` every 100ms for up to 60s; when the DLL appears, calls `HookTaskbarViewDllSymbols`, `Wh_ApplyHookOperations`, then `TriggerInitialScan`
2. Creates `FullscreenPollThread` — waits 1s between fullscreen state checks; with `HideFullscreen=false` (user's setting), it only loops and sleeps, never executing any meaningful code
3. Creates `GsmtcThreadFunc` — initializes a WinRT STA apartment, then blocks on `WaitForSingleObject(startEvent, INFINITE)`
4. Calls `TriggerInitialScan()` — detaches a thread that polls `FindWindowW("Shell_TrayWnd")` every 100ms for up to 30s; when found, posts `WM_SIZE` to trigger `UpdateVisualStates`, then signals the GSMTC start event

---

## The smoking gun: session `02:08:21`

```
[02:08:21.425 tid=10552] Wh_ModInit started
[02:08:21.425 tid=10552] Wh_ModInit: Taskbar.View.dll not loaded — starting poll thread
[02:08:21.425 tid=10684] [poll] PollForTaskbarViewDll: started
[02:08:21.425 tid=10688] FullscreenPollThread: started
[02:08:21.425 tid=10552] Wh_ModInit: GSMTC thread created
[02:08:21.425 tid=10692] GsmtcThreadFunc: started
[02:08:21.425 tid=10552] Wh_ModInit: TriggerInitialScan called — init complete
[02:08:21.425 tid=10696] TriggerInitialScan thread: started, polling for Shell_TrayWnd
[02:08:21.425 tid=10692] GsmtcThreadFunc: init_apartment OK
[02:08:21.425 tid=10692] GsmtcThreadFunc: waiting for start event
```

After these lines — **nothing**. No further BootLog entries from any thread. The next entry is a completely new process starting at `02:09:11`. Explorer crashed somewhere between `02:08:21` and that restart.

At the time of the crash, all four threads are in one of:
- `Sleep(100)` inside a `GetModuleHandleW` poll
- `Sleep(100)` inside a `FindWindowW` poll
- `WaitForSingleObject(event, INFINITE)` — event never signaled
- `WaitForSingleObject(stopEvent, 1000)` loop, doing `continue` because `hideFullscreen=false`

**No substantive user code is executing.** No hooks are installed (UpdateVisualStates hook is never applied since `HookTaskbarViewDllSymbols` was never called). No WinRT calls are in progress. The crash appears to happen with the mod completely idle.

---

## Contrast: session `02:09:11` (same build, same boot sequence)

```
[02:09:11.584 tid=3816] TriggerInitialScan thread: Shell_TrayWnd found, posting WM_SIZE
[02:09:11.615 tid=3816] TriggerInitialScan thread: WM_SIZE posted
[02:09:11.615 tid=3816] TriggerInitialScan thread: signaled GSMTC start event
[02:09:11.615 tid=8320] GsmtcThreadFunc: calling RequestAsync
[02:09:11.884 tid=8320] GsmtcThreadFunc: RequestAsync returned, setting Completed callback
[02:09:14.683 tid=3856] [poll] Taskbar.View.dll detected — hooking symbols
[02:09:14.683 tid=3856] [poll] HookTaskbarViewDllSymbols done
[02:09:14.687 tid=3856] [poll] Wh_ApplyHookOperations done — calling TriggerInitialScan
[02:09:14.689 tid=3856] [poll] TriggerInitialScan called — done
[02:09:14.794 tid=14896] TriggerInitialScan thread: Shell_TrayWnd found, posting WM_SIZE
[02:09:14.794 tid=14896] TriggerInitialScan thread: WM_SIZE posted
[02:09:14.794 tid=14896] TriggerInitialScan thread: signaled GSMTC start event
[02:09:14.794 tid=14896] TriggerInitialScan thread: done
```

This session makes it further — hooks installed, GSMTC active — but also crashes (user confirms still crashing; log ends here with no further restart logged by the user).

---

## History of eliminated hypotheses

1. **`LoadLibraryExW` hook trampoline race** — removed entirely; crash persists
2. **`Wh_ApplyHookOperations` called from inside hook callback** — deferred to background thread; didn't help
3. **GSMTC `RequestAsync` on uninitialized WinRT service** — fixed by making TriggerInitialScan wait for `Shell_TrayWnd` before signaling GSMTC; partially helps but crash at `02:08:21` happens before GSMTC is ever signaled
4. **`GetFrameworkElementFromNative` QI on bad vtable slot** — fixed with `VirtualQuery`+`MEM_IMAGE` guard; resolved enable/disable crash

---

## Questions for analysis

1. In session `02:08:21`, all four threads are sleeping and no hooks are installed. What could be crashing `explorer.exe` under those conditions? Is there any mechanism by which Windhawk's presence in the process — even with an idle mod — could cause an unhandled exception on cold boot?

2. The 100% cold-start-vs-direct-path correlation suggests the timing of Windhawk injection relative to Explorer's own initialization is the variable, not our code itself. What Explorer initialization sequences are known to be hazardous for injected DLLs on Windows 11 24H2? Are there known race conditions between Windhawk's DLL injection (remote thread via `CreateRemoteThread` or APC) and Explorer's early boot DLL loading?

3. `init_apartment(apartment_type::multi_threaded)` is called in `Wh_ModInit`. On cold boot before COM is fully initialized, could this call — even wrapped in `try`/`catch` — cause process-wide COM state corruption that manifests as a crash tens of seconds later?

4. `GsmtcThreadFunc` calls `init_apartment(apartment_type::single_threaded)` and then blocks. On cold boot, could creating a new STA on an injected thread interfere with Explorer's own COM apartment setup?

5. Is there a known Windows 11 behavior where Explorer terminates itself (`ExitProcess` or `TerminateProcess`) during boot if it detects unexpected state — e.g., a foreign apartment, an unexpected thread in its process, unexpected changes to its module list? Could Windhawk's injection itself trigger such a self-termination?

6. Given the session `02:09:11` where the mod is fully active and the crash still occurs after hooks are installed — what are the likely crash sites in the hook/injection path (`GetFrameworkElementFromNative`, `ScheduleScanAsync`, `InjectWidgetInto`) that would produce an unhandled `EXCEPTION_ACCESS_VIOLATION` not catchable by C++ `catch(...)`?

7. What specific Windows Event Log entries, WER (Windows Error Reporting) crash dumps, or other persistent OS-level artifacts should we inspect to identify the faulting module, exception code, and stack trace for these crashes — given that WinDbg isn't available in this environment?

---

## Constraints for your response

- Do NOT produce any code edits or code rewrites
- Do NOT produce modified versions of any functions
- Analysis, hypotheses, and diagnostic recommendations only
- If recommending additional logging, describe what to log and where — do not write the code
