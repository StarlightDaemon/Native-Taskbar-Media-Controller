# Research Prompt: SMTC Compatibility Audit
**Project:** Native Taskbar Media Controller (Windhawk mod)  
**Date:** 2026-05-24  
**Version under review:** v1.0.0  
**Primary source file:** `native-taskbar-media-controller.wh.cpp` (root of this repo)  
**Supplementary research doc:** `compass_artifact_wf-da455044-6fe7-4744-b095-1f19a6447ff0_text_markdown.md` (root of this repo)  

---

## Background

This is a Windhawk mod (`explorer.exe`) that injects a native XAML media controller widget
into the Windows 11 taskbar using the Global System Media Transport Controls API (GSMTC).
It reads `GlobalSystemMediaTransportControlsSessionManager`, displays now-playing metadata,
and exposes playback controls for any SMTC-registered media app.

The mod classifies each GSMTC session into one of three `SessionSource` buckets
(defined at line ~225 of the .wh.cpp):

```cpp
enum class SessionSource {
    Unknown = 0,
    NativeApp,      // Win32 or Store app (Spotify, WMP, Libby, …)
    Browser,        // Chromium-family (Chrome, Edge, Brave, Opera, Vivaldi, Arc, Thorium, Chromium)
    BrowserFirefox, // Firefox — never exposes timeline data (Mozilla bug 1689538)
};
```

Classification runs via `ClassifySessionSource()` (line ~438), which parses the session's
AUMID string through `ExtractExeHint()` (line ~410) to get a lowercased exe stem, then
matches against a hardcoded browser stem list.

Audiobook mode is set via a single field in `MediaState` (line ~242):
```cpp
bool isAudiobook = false;  // true when durationMs > 1 hour
```

---

## Issues to Investigate

Work through each issue below. For each one, read the relevant source sections, then
answer the specific research questions. Quote file lines where they support your answer.

---

### Issue 1 — Firefox documentation contradicts code and external research

**The contradiction:**
- In-mod readme (embedded in .wh.cpp, lines ~51–52) states:
  > Firefox: fully supported — media info and timeline data are exposed by current Firefox versions.
- The `SessionSource` enum comment (line ~229) states:
  > `BrowserFirefox, // Firefox — never exposes timeline data (Mozilla bug 1689538)`
- The supplementary research doc states Firefox does not expose timeline, citing
  Mozilla Bugzilla 1689538 as unresolved.

**Research questions:**
1. Read the source and confirm: does any timeline-suppression code path gate on
   `source == SessionSource::BrowserFirefox`? Or does the code rely solely on
   `durationMs == 0` (which Firefox would naturally send)?
2. The in-mod readme says Firefox timeline "is exposed." The code says it never is.
   Which is empirically correct for Firefox on Windows 11 as of mid-2026?
   Specifically: has Mozilla Bugzilla 1689538 been resolved in any Firefox release
   shipped before 2026-06-01?
3. If the readme is wrong, describe the exact correction needed (which lines, what text).

---

### Issue 2 — Chromium browser timeline: readme claims availability, research says none

**The contradiction:**
- In-mod readme (lines ~49–50) states:
  > Timeline data is available for most Chromium-based browsers.
- The supplementary research doc states, under "Browser / PWA behavior":
  > Field coverage in browsers... Timeline (position/duration) = ❌ in both Chromium and Firefox
- The `SessionSource::Browser` classification exists but no timeline-suppression code
  appears to gate on it (unlike `BrowserFirefox`).

**Research questions:**
1. Search `ApplyStateToWidget()` and all timeline/progress update paths in the source.
   Is there any code that suppresses timeline display for `SessionSource::Browser`?
2. If Chromium browsers genuinely never expose timeline data (as the research doc states),
   is the current `durationMs == 0` guard sufficient to keep the progress bar hidden for
   browser sessions — or is there a path where a browser could send a non-zero duration
   and get a progress bar incorrectly?
3. Do any known Chromium-based browsers on Windows 11 (Chrome 120+, Edge 120+) actually
   expose non-zero `TimelineProperties` via SMTC? Confirm or deny this claim. The answer
   determines whether the readme is wrong, whether the research doc is wrong, or whether
   "most Chromium browsers" in the readme refers to a specific subset.
4. If Chromium browsers do NOT expose timeline, what is the correct readme text?

---

### Issue 3 — Audiobook detection is one-dimensional

**Current implementation (line ~242):**
```cpp
bool isAudiobook = false;  // true when durationMs > 1 hour
```
When `isAudiobook == true`, the UI:
- Relabels skip buttons as "Previous chapter" / "Next chapter" (lines ~982–988)
- Relabels the artist TextBlock accessibility name as "Author" (line ~947)
- Shows skip-back/forward buttons based on `canSkipBackward` / `canSkipForward` flags
- Shows playback rate suffix (e.g. "· 1.5×") in the artist row (lines ~922–928)

**The problem:**
Short audiobook chapters (common in Libby, Audible Cloud Player, and some Audiobookshelf
titles) can be under one hour, so `durationMs > 3,600,000` never triggers. Such a session
would be treated as NativeApp music and the chapter-navigation semantics would be wrong.

**Research questions:**
1. Read the full `UpdateOneSession` or equivalent metadata ingestion path in the source.
   Find exactly where `isAudiobook` is set. Is there any secondary heuristic beyond
   the duration check, or is it strictly and solely `durationMs > 1 hour`?
2. The supplementary research doc describes richer audiobook detection heuristics:
   "Chapter NN" / "Part NN" in title, or artist field = author-name patterns.
   Evaluate each heuristic against the SMTC fields actually available
   (`Title`, `Artist`, `AlbumTitle`, `AlbumArtist`) for the confirmed audiobook sources
   (Libby via browser, Audiobookshelf via browser, Audible Cloud Player via browser).
   For each heuristic, state: (a) is the required SMTC field reliably populated by
   these sources, and (b) is the heuristic likely to produce false positives for
   non-audiobook apps?
3. Propose a revised `isAudiobook` determination that would correctly detect short
   Libby chapters without misclassifying a 30-minute music album track. The
   proposal should only use fields already present in `MediaState` (no new SMTC calls).

---

### Issue 4 — Radio mode: no detection, no dedicated UI state

**Current behavior:**
Skip buttons are collapsed when `canSkipForward == false` (line ~977–980). This is
correct. But there is no explicit "radio mode" state and no suppression of the progress
bar specifically for live radio sessions.

**The gap:**
For live radio sessions (iHeartRadio, TuneIn, BBC Sounds):
- `IsPreviousEnabled == false` and `IsNextEnabled == false` (confirmed by research doc)
- `durationMs` is 0 for live streams (progress bar correctly hidden by existing guard)
- The `isAudiobook` flag would be false

So skip buttons are already correctly hidden and the progress bar is already correctly
hidden. The research doc proposes adding explicit "Stop / Listen Live" UI for radio
sessions, but that is a new feature scope item, not a defect.

**Research questions:**
1. Confirm: is there any currently-playing radio scenario where the existing
   `canSkipForward`/`canSkipBackward` collapse logic would fail — i.e., a live radio
   app that reports `IsNextEnabled == true` even though there is no meaningful "next"?
   Check TuneIn and iHeartRadio specifically against the research doc's compatibility table.
2. Is there any case where a live radio stream exposes a non-zero `durationMs`
   (making the progress bar visible on a live stream), based on the sources in the
   research doc?
3. Given the answers above, state whether radio mode is a correctness gap requiring
   a fix before SC-SP-1, or a scope addition that can be deferred.

---

### Issue 5 — AUMID parsing edge cases: newer and niche browsers

**Current browser stem list (line ~441–444):**
```cpp
static const std::wstring kBrowserStems[] = {
    L"chrome", L"msedge", L"brave", L"opera", L"operagx",
    L"vivaldi", L"arc", L"thorium", L"chromium",
};
```

**Research questions:**
1. Are there Chromium-family browsers with significant Windows market share that use
   an exe name NOT in this list? Consider: Whale (Naver), Cent Browser, SlimBrowser,
   Waterfox (Gecko, not Chromium), Pale Moon (Gecko). For each, state its engine and
   whether its Windows exe name would match anything in the list.
2. For browsers that use a Store AUMID format (e.g., Edge: 
   `Microsoft.MicrosoftEdge.Stable_8wekyb3d8bbwe!App`), does `ExtractExeHint()` 
   correctly extract `msedge` from the Store AUMID? Trace the function logic step 
   by step against that example AUMID.
3. Arc Browser's AUMID format on Windows is not well-documented. If Arc's Store AUMID
   post-bang AppId is something other than `arc`, would it fall through to `NativeApp`?
   What is the consequence of misclassifying Arc as NativeApp?

---

### Issue 6 — SessionSource.Unknown race window

**The issue:**
`MediaState.source` is initialized to `SessionSource::Unknown` (line ~250). It is set
at enumeration time during `UpdateSessions()` / `UpdateOneSession()` (or equivalent).

If `ApplyStateToWidget()` runs between when a new session is added to `g_MediaStates[]`
and when `ClassifySessionSource()` populates its `.source` field, the session would have
`source == Unknown` and any feature gate on source type would silently fall through to
the default behavior.

**Research questions:**
1. Read `UpdateSessions()` or the session enumeration path in full. Is `.source` set
   atomically as part of constructing or inserting the `MediaState`, or is there a
   window where a partially-initialized `MediaState` with `source == Unknown` is
   visible to other threads?
2. Is there a future feature (specifically SC-SP-1 seek bar) that would add a hard
   gate like `if (source == BrowserFirefox) { hideSeekBar(); }` — where `Unknown`
   falling through would incorrectly show a seek bar for a newly-classified browser?
3. State whether the current initialization order is safe, or recommend the exact
   change needed to ensure `.source` is always set before the `MediaState` becomes
   visible to `ApplyStateToWidget`.

---

## Deliverables

For each issue, provide:
- **Finding:** what the code actually does (with line references)
- **Verdict:** is this a defect, a documentation error, a latent risk, or non-issue
- **Recommendation:** the minimal change required, or "no change needed" with justification

Prioritize findings by impact on the next planned feature (SC-SP-1: interactive seek bar)
since that feature must gate on session source type to suppress scrubbing for browser
and Firefox sessions.

Do not propose new features. Do not refactor code outside the scope of each issue.
Output findings as a structured report, one section per issue.
