# Libby Audiobook Support — Review & Refinement Handoff

**Date:** 2026-05-22  
**Target file:** `native-taskbar-media-controller.wh.cpp`  
**Branch:** `main` (from `0155fa3`)

---

## Context

This mod injects a XAML media-controller widget into the Windows 11 taskbar
using Windhawk. It reads session data from the Global System Media Transport
Controls (GSMTC) API and displays: Title, Artist, play/pause toggle, and a
next-track button. It supports multiple sessions with a cycle chip.

Libby (by OverDrive) is a Windows Store audiobook/library app. Its SMTC
integration maps audiobook metadata differently from music apps — the current
display logic was designed for music (Spotify, YouTube Music) and degrades for
audiobooks. This handoff covers what to investigate and potentially implement to
make Libby a first-class session.

---

## Current State (what we read today)

In `UpdateOneSessionAsync` (around line 620), the only fields fetched from
`GlobalSystemMediaTransportControlsMediaProperties` are:

```cpp
title  = props.Title().c_str();
artist = props.Artist().c_str();
```

And from `GetPlaybackInfo`:
```cpp
playing = (pb.PlaybackStatus() == ...::Playing);
```

The `MediaState` struct stores only: `title`, `artist`, `isPlaying`, `sessionId`
(AUMID), and the two event tokens.

**Fields available in GSMTC that we do not currently read:**

| Field | API | Audiobook meaning |
|---|---|---|
| `AlbumTitle` | `props.AlbumTitle()` | Book title (when `Title` = chapter name) |
| `AlbumArtist` | `props.AlbumArtist()` | Author (sometimes populated instead of `Artist`) |
| `Subtitle` | `props.Subtitle()` | Chapter subtitle or part number |
| `TrackNumber` | `props.TrackNumber()` | Chapter number |
| `PlaybackRate` | `pb.PlaybackRate()` | Listen speed (1.0 / 1.25 / 1.5 / 2.0) |
| `TimelineProperties` | `session.GetTimelineProperties()` | Position + duration (needed for seek bar) |
| `PlaybackType` | `pb.PlaybackType()` | Enum: Music / Video / Image / Unknown |
| `AutoRepeatMode` | `pb.AutoRepeatMode()` | Not relevant for audiobooks |

---

## Phase 1 — Research (must complete before any code changes)

### 1a. Determine Libby's AUMID

The mod stores `SourceAppUserModelId` in `m.sessionId` (line 691). To identify
Libby's AUMID, run the mod with Windhawk logging enabled while Libby is playing,
then look for the `[gsmtc] enumerated` log line — it will print `active=N` where
N is the session index. The AUMID is logged during `DoEnumerateAndRefresh`.

Alternatively, query from PowerShell:
```powershell
Get-AppxPackage | Where-Object { $_.Name -match -i 'libby|overdrive' }
```
The `PackageFamilyName` is typically the AUMID prefix. Expected pattern:
`OverDrive.Libby_<hash>` or `OVERDRIVE.MediaConsole_<hash>`.

**Record the AUMID in this document once confirmed.**

### 1b. Audit what Libby actually populates over SMTC

With Libby playing an audiobook, use a test script or the Windows SDK
`MediaTimelineController` to dump all SMTC properties for the session. Key
questions:

- Does `Title` contain the book title, the chapter title, or both?
- Is `AlbumTitle` populated? With what?
- Is `Artist` the author name or empty?
- Does `AlbumArtist` differ from `Artist`?
- Is `GetTimelineProperties()` non-null? Does it return a meaningful position
  and duration?
- What `PlaybackType` does Libby report? (Some audiobook apps report `Music`,
  others `Unknown`.)
- Are `SkipForward` / `SkipBackward` in `GetPlaybackInfo().Controls`?
  (These are the 30-second skip buttons, more useful than NextTrack for
  audiobooks.)

A quick PowerShell dump (Windows.Media.Control API via .NET):
```powershell
Add-Type -AssemblyName System.Runtime.WindowsRuntime
# Then use [Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager] ...
```
Or use a small C# console app targeting `Windows.Media.Control`.

### 1c. Check for Libby-specific SMTC quirks

Known issues with audiobook apps on SMTC:
- Some apps fire `MediaPropertiesChanged` on every chapter change but not on
  resume — verify the mod's event subscription catches chapter transitions.
- Some apps set `Title` to a blank string between chapters during buffering —
  the mod currently hides the widget when `title.empty()` (line 426); verify
  this doesn't cause a flash-of-nothing between chapters.

---

## Phase 2 — Implementation Candidates

Prioritize based on Phase 1 findings. Items are listed from highest to lowest
impact.

### 2a. Smart title/artist fallback for audiobooks (low complexity)

**Problem:** For music, `Title` = song name, `Artist` = performer. For
audiobooks, `Title` may be the chapter name (less useful to see in a small
widget) and `AlbumTitle` may be the book name (more useful). `Artist` may be
empty while `AlbumArtist` has the author.

**Proposed logic in `UpdateOneSessionAsync`:**
```cpp
title  = props.Title().c_str();
artist = props.Artist().c_str();

// Audiobook fallback: prefer book title over chapter title for the primary line,
// and author from AlbumArtist if Artist is empty.
auto albumTitle  = std::wstring(props.AlbumTitle().c_str());
auto albumArtist = std::wstring(props.AlbumArtist().c_str());

if (!albumTitle.empty() && title.empty()) title = albumTitle;      // no chapter name at all
if (artist.empty() && !albumArtist.empty()) artist = albumArtist;  // author fallback
```

This is a safe, non-breaking change for music sessions (AlbumTitle is usually
empty or redundant for music). Apply unconditionally — no AUMID matching
required.

If Phase 1 reveals that Libby puts the chapter name in `Title` and the book
name in `AlbumTitle`, consider swapping them (show book name as primary, chapter
as secondary):
```cpp
// If AlbumTitle is set and Title looks like a chapter, prefer AlbumTitle as
// the primary display line and demote Title to the artist slot.
if (!albumTitle.empty() && !title.empty() && artist.empty()) {
    artist = title;      // chapter name moves to subtitle row
    title  = albumTitle; // book name is the headline
}
```
Only implement this swap if Phase 1 confirms it's the right mapping for Libby.

**Files to change:** `MediaState` struct (add `albumTitle`, `albumArtist`
fields if needed), `UpdateOneSessionAsync`, `ApplyStateToWidget`.

### 2b. Playback rate display (low complexity)

Audiobook users routinely listen at 1.5× or 2×. Show the rate in a small label
when it's not 1.0:

```cpp
double rate = 1.0;
if (auto rateVal = pb.PlaybackRate()) rate = rateVal.Value();
// Store in MediaState, display as "1.5×" suffix on artist row when rate != 1.0
```

**Files to change:** `MediaState` struct (add `playbackRate` double),
`UpdateOneSessionAsync`, `ApplyStateToWidget` (append to artist TextBlock text,
or add a third label).

### 2c. SkipForward / SkipBackward buttons (medium complexity)

Libby exposes 30-second skip commands via SMTC controls. These are more
relevant to audiobook listeners than the next-track button. Before implementing,
check `pb.Controls().IsSkipForwardEnabled()` and
`pb.Controls().IsSkipBackwardEnabled()` from Phase 1.

If enabled, add ⏩/⏪ (or `«` / `»`) buttons alongside or replacing the current
next-track button. Use `session.TrySkipNextAsync()` / `TrySkipPreviousAsync()`
— or `TrySkipForwardAsync()` / `TrySkipBackwardAsync()` if those are exposed
on the session object (check the GSMTC API surface).

**Files to change:** `BuildWidget()` (add buttons to XAML tree),
`ApplyStateToWidget` (wire button click handlers or update visibility based on
which controls are enabled).

### 2d. Seek bar — reference SC-SP-1

The seek bar synthesis candidate (SC-SP-1, from memeri121's fork) is the
highest-impact audiobook feature overall. It requires `GetTimelineProperties()`
for position/duration and a `Slider` element in the XAML tree. This is tracked
as a separate Phase 2 candidate in GOALS.md and is not in scope for this
handoff — but Phase 1 should confirm that Libby's `TimelineProperties` is
populated, which will inform the priority of SC-SP-1.

---

## Phase 3 — Testing Checklist

- [ ] Libby playing: widget shows book title and author (not chapter ID and
      empty artist)
- [ ] Chapter change: widget updates without flash-of-nothing
- [ ] Libby paused: widget shows pause state correctly
- [ ] Libby at 1.5×: playback rate shown (if 2b implemented)
- [ ] Music session (Spotify): unaffected by fallback logic — Title/Artist
      display unchanged
- [ ] Multiple sessions (Libby + Spotify simultaneously): session cycle chip
      works, both sessions display correctly
- [ ] Mod unload while Libby is playing: clean unload, no crash

---

## Known Libby Limitation — Cover Art

Libby does not expose cover art via the standard SMTC thumbnail channel
(`props.Thumbnail()`). The Windows media flyout shows no art for Libby sessions,
confirming this. When cover art display is implemented in the mod (related to
SC-UI-1 blurred album art background), Libby sessions should gracefully fall
back to a placeholder or no-art state — do not treat a null/unreadable
`Thumbnail()` stream as an error. All other apps (Spotify, YouTube Music, etc.)
are expected to populate `Thumbnail()` correctly.

Note: cover art is not yet implemented in the widget at all. This note is
pre-emptive, to be acted on when that feature lands.

---

## Constraints & Gotchas

- **No AUMID hardcoding unless absolutely necessary.** The fallback logic in 2a
  should work generically. Only add Libby-specific branching on AUMID if generic
  logic produces wrong output for music sessions.
- **`TryGetMediaPropertiesAsync` is already `co_await`-ed** — adding more field
  reads from `props` is free (no extra async calls needed).
- **`GetTimelineProperties()` is synchronous** — safe to call in the same
  coroutine after the `co_await`.
- **`WH_LOG_CATCH` macro** is defined at the top of the file — use it for all
  new try/catch blocks.
- **Do not add new GSMTC event subscriptions** unless Phase 1 identifies a
  specific gap. `MediaPropertiesChanged` already covers chapter transitions.

---

## Files to Read Before Starting

1. `native-taskbar-media-controller.wh.cpp` — full file (~1043 lines)
2. `.raiden/state/CURRENT_STATE.md` — phase status and what already works
3. `.raiden/state/GOALS.md` — Phase 2 candidate list (SC-SP-1 seek bar context)
4. `fork-reports/fork-review-memeri121-taskbar-spotify-widget-2026-05-19.md` —
   SC-SP-1 seek bar implementation notes (for Phase 1 timeline property audit)

## Commit Convention

One commit per logical change. Message format:
```
feat: <what> for <why>

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
```
