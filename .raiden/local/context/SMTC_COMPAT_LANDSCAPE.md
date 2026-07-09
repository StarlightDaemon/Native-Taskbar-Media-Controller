# Windows SMTC / GSMTC Compatibility Landscape for Audio-Focused Apps

## TL;DR
- **Roughly three behaviors dominate:** (1) Native UWP/MSIX apps that auto-register a clean SMTC session via `Windows.Media.MediaPlayer` (Spotify, Apple Music, Tidal, Amazon Music, Groove/Media Player, iHeartRadio, TuneIn, VLC-UWP); (2) Chromium/Edge/Firefox browser hosts that surface a single per-browser SMTC session derived from the W3C Media Session API (YouTube Music PWA, SoundCloud web, Pandora, Libby, Audible Cloud Player, BBC Sounds, Audiobookshelf, Libro.fm, virtually every podcast/radio web player); and (3) Win32 apps that need third-party plugins (foobar2000, MusicBee, AIMP, Winamp 2024 revival, VLC desktop 3.x, iTunes, DeaDBeeF, MPV).
- **Tier-1 special-case targets for a Windhawk taskbar media controller:** Spotify (the only mainstream service with all SMTC fields populated), Apple Music (no shuffle/repeat exposed), Chromium/Edge browser sessions (no timeline, shuffle, repeat, stop), Firefox (no timeline at all — Bugzilla 1689538), Tidal (album field empty due to Chromium issue 349310439), Libby/Audiobookshelf/Audible Cloud Player (audiobook-shaped metadata where "Artist" = author and prev/next = chapter), and Windows Media Player / Media Player (title/artist often blank).
- **Tier-3 / non-actionable:** Audible native app (discontinued January 13, 2022; stopped working July 31, 2022), NPR One (sunset December 13, 2023), Apple Podcasts / Calm / Headspace / Overcast / official Brain.fm desktop / official Noizio (no first-party Windows native), Audacious / Strawberry / Quod Libet / PotPlayer / KMPlayer / GOM / Kodi-UWP / QQ Music (no SMTC by any means per the ModernFlyouts compatibility table).

## Key Findings

### How SMTC is populated, in one paragraph
GSMTC (`Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager`) aggregates sessions from any process registered with the underlying `SystemMediaTransportControls` interface. UWP/WinUI apps using `MediaPlayer` are auto-integrated since Windows 10 1607; Win32 apps must use `ISystemMediaTransportControls` / `ISystemMediaTransportControls2` directly (or via C++/WinRT). Each session carries: `Title`, `Artist`, `AlbumTitle`, `AlbumArtist`, `TrackNumber`, `Genres`, `Thumbnail` (a `RandomAccessStreamReference` — embedded bitmap stream, **not** a URL), `PlaybackType` (Music/Video/Image), playback status, and (only if the app supplies them) timeline properties (`StartTime`, `EndTime`, `Position`, `MinSeekTime`, `MaxSeekTime`). Control capability is advertised via `IsPlayEnabled`, `IsPauseEnabled`, `IsPreviousEnabled`, `IsNextEnabled`, `IsRewindEnabled`, `IsFastForwardEnabled`, `IsStopEnabled`, `IsRepeatEnabled`, `IsShuffleEnabled`, `IsChannelDownEnabled`, `IsChannelUpEnabled`. A `RequestedPlaybackRate` event exists, but most apps never call it — playback rate is therefore essentially invisible to SMTC consumers.

### Browser / PWA behavior (critical)
- **Chromium hosts (Chrome, Edge, Brave, Opera, Vivaldi)** register exactly **one SMTC session per browser process / user profile**, not per tab and not per origin. The app identity reported in SMTC is the browser itself ("Chrome", "Microsoft Edge"). When audio in another tab starts, the existing session is reassigned (title/artist/art change) rather than a second session opening. **This is the single most important quirk for taskbar-media-controller authors.**
- **PWA installs:** A PWA opened in standalone window mode (Edge "Install as app", Chrome "Install YouTube Music") still runs inside the parent browser's process tree and **does not get a distinct SMTC source identity by default** on current Windows builds. The app name shown is still the host browser; differentiation requires inspecting `MediaMetadata` content.
- **Field coverage in browsers (per the ModernFlyouts-Community compatibility table):** Play/Pause, Previous, Next, Thumbnail, Media Title, Media Artist = ✅. App Info = ⚠️ partial. Shuffle, Repeat, Stop, **Timeline (position/duration) = ❌** in both Chromium and Firefox.
- **Tor and Pale Moon do NOT register SMTC sessions at all.**
- **Firefox** registers SMTC similarly to Chromium (single session, no timeline). Firefox uses an invisible HWND bound to `ISystemMediaTransportControls` internally; the unresolved timeline gap is Mozilla Bugzilla **1689538**.

### Native Win32 apps without built-in SMTC
foobar2000, MusicBee, AIMP, Winamp (2024 revival), VLC desktop, iTunes, DeaDBeeF, MPV, and Spicetify-modded Spotify rely on community plugins. The major plugins are: `foo_mediacontrol` (foobar2000, dumbie), `mb_MediaControl` (MusicBee, ameer1234567890/HenryPDT), AIMP's official add-on (aimp.ru catalog id 1097), `gen_smtc`/`gen_smtcinterop`/`gen_w10mc` (Winamp), `vlc-win10smtc` by spmn (VLC ≤ 3.0.x), `iTunes-SMTC` by thewizrd, `ddb_smtc` (DeaDBeeF), MPVMediaControl (datasone) / MPV-SMTC (x0wllaar). **VLC desktop has had an upstream merge request `!3010` adding native `win_smtc` since 2023**, but stable users on VLC 3.0.x still need the plugin.

### Audiobook & podcast realities
- **Audible's native Windows UWP app was discontinued January 13, 2022 and stopped working July 31, 2022.** Per Audible's own customer email, quoted by Softpedia (January 2022) and gHacks (January 19, 2022): *"We discontinued this app on January 13, 2022, and it is no longer available in the Windows App Store … You can continue to use your current app to listen until July 31, 2022. After that date, you won't be able to use your current app to access Audible or your library."* The Amazon Appstore-on-Windows-11 fallback path (Android apps via WSA) became unavailable for new installs on **March 6, 2024**, with full end-of-support on **March 5, 2025**, per Amazon's own developer bulletin: *"Starting on March 6, 2024, Windows 11 customers will not be able to search for Amazon Appstore or associated apps from the Microsoft Store … Amazon Appstore on Windows and all applications and games dependent on WSA will no longer be supported beginning March 5, 2025."* The only first-party Windows path today is **Audible Cloud Player in a browser**, which surfaces only as a Chromium SMTC session.
- **Libby is browser-only on Windows** (no UWP, no MSIX, no Win32). The Libby help page explicitly directs Windows users to `libbyapp.com`. SMTC presence is via Chromium/Edge Media Session integration; the iOS lock-screen equivalent is confirmed, making a Media Session implementation on the web client highly likely.
- **Audiobookshelf**'s regular web player **uses the Media Session API** (confirmed in advplyr/audiobookshelf GitHub issue #3768: *"When playing a shared audiobook the audio player doesn't use the media session API … In the regular audio player the media session API is used."*). The "shared audiobook" link player skips it — a known bug.
- **Pocket Casts Desktop Apps Version 2.0 launched June 25, 2024**, per the official Pocket Casts Blog: *"We are happy to announce version 2.0 of the Pocket Casts Desktop App for Windows and Mac, available to our Plus/Patron users … Both our Mac and Windows desktop apps have been rewritten in Electron."* The legacy (pre-Electron) app supported media keys and Windows lock-screen controls per the Ctrl.blog review; the Electron v2 retains media-key support, but multiple July 2024 forum threads report users losing the prior taskbar thumbnail buttons. The Microsoft Store build of Pocket Casts is currently unavailable per Pocket Casts Support.
- **NPR One was sunset December 13, 2023**, per NPR's own press release "NPR Unifies Mobile App Experience for Listeners, Sunsets NPR One App" (npr.org, December 13, 2023): *"The new NPR app is now your one-stop mobile app destination for the best of public radio. It brings together the best features from our previous mobile app experiences — NPR One and the previous NPR app."* The new NPR app is iOS / Android only, with no Windows desktop client.
- **Apple Podcasts has no Windows app.** Apple Music on Windows handles music, lossless, and Dolby Atmos (and adds media-key/SMTC support since the April 25, 2023 update) but not podcasts.
- **Overcast is iOS-only**; overcast.fm web player lacks media-key/SMTC integration per third-party wrapper "Undercast"'s premise. **Podbean** is browser/Android only. **Calm, Headspace, Brain.fm, Noizio** have no first-party Windows native apps; the "desktop apps" listed in WebCatalog and similar directories are third-party PWA wrappers.
- **Endel** does have a first-party Microsoft Store UWP app; SMTC integration is likely (UWP convention) but unverified in public reports.
- **Duolingo** has a Microsoft Store UWP app but plays short audio snippets per lesson; it is not expected to register a persistent SMTC session, and no user reports document one.
- **Pimsleur** has no native Windows app — only `learn.pimsleur.com` and a legacy "Course Manager" downloader that is not a player.
- **Speechify launched its native Windows app (WinUI3/MSIX, Microsoft Store) on March 31, 2026**, per its official PRWeb press release: *"Speechify, the world's most used text-to-speech app, today announced the launch of its Windows application, bringing real-time text-to-speech and voice typing to Windows users with the option of fully on-device processing."* Also covered by TechCrunch (March 31, 2026). SMTC behavior is undocumented at this report's date.

### Known platform-specific quirks worth special-casing
1. **Tidal on Windows omits the album name** — its desktop client is Chromium/Electron-based and is hit by Chromium issue 349310439. Title and artist arrive; `AlbumTitle` is blank. (Reported in ungive/discord-music-presence #211: *"TIDAL on Windows does not report album names, this is due to a long-standing Chromium bug that does not seem to get fixed anytime soon."*)
2. **Spotify is the gold standard** — full play/pause/prev/next/shuffle/repeat/stop/timeline/thumbnail/title/artist; app info partial.
3. **Apple Music Preview for Windows added media-key and Windows media-controls support in the update released April 25, 2023**, per 9to5Mac: *"Windows media controls and keyboard shortcuts now work,"* and MacRumors (same date): *"the Apple Music Preview app now appears to include support for Windows 11 media controls and keyboard shortcuts."* Shuffle/repeat are not exposed to SMTC (per the FluentFlyout compatibility note: *"Apple Music (lacks shuffle/repeat support)"*).
4. **MediaMonkey** registers SMTC but **does NOT send a thumbnail** (per ModernFlyouts table).
5. **TuneIn Radio (UWP)** sets a thumbnail and title but has `IsPreviousEnabled` / `IsNextEnabled` = false (radio streams have no track navigation) and `Artist` field is partial.
6. **Crunchyroll UWP** registers a session with title/artist/thumbnail all missing — only play/pause works.
7. **Movies & TV, Media Player (Win 11 Groove successor)** register sessions but with thin metadata — Media Player notably **leaves title and artist blank in the SMTC session** while still exposing timeline.
8. **Amazon Music** for Windows has documented media-key support (e.g. via Logitech SetPoint configurations) but per-field SMTC coverage is not well-documented in community sources.
9. **Audiobookshelf's "share" player** skips Media Session — Bluetooth/SMTC will be blank when playing a shared audiobook link.
10. **iTunes** locks media keys on its window, preventing other apps from receiving them while iTunes is open (well-documented complaint on Apple Discussions); the `iTunes-SMTC` plugin works around this but the root behavior remains.

### Popularity tiers (audience reach)
- **Mainstream major:** Spotify, YouTube Music, Apple Music, Amazon Music, Audible (mobile, since the Windows app is dead), VLC, Windows Media Player / Media Player, Pandora (US), SoundCloud, iHeartRadio (US), TuneIn, BBC Sounds (UK), Libby (US/Canada library users), Pocket Casts, Calm, Headspace, Duolingo.
- **Active niche:** Tidal, Deezer, Qobuz, foobar2000, MusicBee, AIMP, Winamp revival, MediaMonkey, MPC-HC, Pimsleur, Libro.fm, Audiobookshelf, Brain.fm, Endel, Speechify.
- **Very niche / legacy:** DeaDBeeF, Audacious, Strawberry, Quod Libet, MusicPlayer2, Nugs.net, Bandcamp (browser-only), Radio.garden (browser-only), NPR One (discontinued December 13, 2023), Overcast (no Windows), Podbean (no Windows native), Noizio (no Windows native).

## Details

### Master Table

| App | Category | Windows Availability | SMTC Registration | Metadata Quality | Navigation Controls | Known Quirks | Popularity |
|---|---|---|---|---|---|---|---|
| **Spotify (native)** | Music | Win32 + Microsoft Store | ✅ Built-in (full) | Title, Artist, Thumbnail, Timeline, App info partial | Prev/Next + Shuffle/Repeat/Stop | Only mainstream service with **all** SMTC fields populated | Mainstream major |
| **Spotify (web/PWA)** | Music | Browser PWA | ✅ via browser session | Title, Artist, Thumbnail | Prev/Next | Reports under browser AUMID, not "Spotify" | Mainstream major |
| **Apple Music (native)** | Music | UWP Microsoft Store (since 2023) | ✅ Built-in since the April 25, 2023 update (9to5Mac/MacRumors confirmed) | Title, Artist, Album, Thumbnail, Timeline | Prev/Next | **No shuffle/repeat in SMTC** (FluentFlyout: "lacks shuffle/repeat support") | Mainstream major |
| **YouTube Music (PWA)** | Music | Chromium/Edge PWA | ✅ via browser session | Title, Artist, Thumbnail | Prev/Next | Reports under browser AUMID; third-party wrappers (ytmdesktop, th-ch/youtube-music) give a distinct identity | Mainstream major |
| **Amazon Music** | Music | Native Win32 (download from Amazon) | ✅ media-key routing confirmed (Logitech SetPoint workarounds, 2017–2021); SMTC integration documented but field coverage thin in community sources | Title, Artist, Thumbnail | Prev/Next | Older versions had imperfect media-key auto-detection | Mainstream major |
| **Tidal** | Music | Native Win32 (Chromium/Electron-based) | ✅ via embedded Chromium SMTC | Title, Artist, Thumbnail; **no Album** | Prev/Next | **Album field is blank** on Windows due to Chromium issue 349310439 (ungive/discord-music-presence #211) | Mainstream major |
| **Deezer** | Music | Native Win32 + browser | ✅ Built-in (covered by S Media Controls compatibility list) | Title, Artist, Thumbnail | Prev/Next | Treated as standard music session | Active niche |
| **Pandora** | Music | Browser/PWA only on Windows | ✅ via browser | Title, Artist, Thumbnail | Prev/Next (no true "prev" for radio mode) | US-only service | Mainstream (US) |
| **SoundCloud** | Music | Microsoft Store UWP wrapper (9NVJBT29B36L) + browser | ✅ Built-in for the Store app | Title, Artist, Thumbnail, Stop; no shuffle/repeat/timeline | Prev/Next | UWP wrapper is essentially a chromed web shell but appears to SMTC as "SoundCloud" | Mainstream major |
| **Qobuz** | Music (HiFi) | Native Win32 | Unconfirmed in public sources | Likely full | Likely full | Treat like Tidal/Deezer in absence of contradiction | Active niche |
| **Nugs.net** | Music (live) | Browser + mobile-first | Browser session only | Title, Artist, Thumbnail | Prev/Next | Niche live-recording service | Very niche |
| **Bandcamp** | Music (DTC) | Browser only on Windows | ✅ via browser Media Session | Title, Artist, Thumbnail | Prev/Next | Per-tab session; album art reliable | Active niche |
| **foobar2000** | Local | Win32 | 🟡 **Plugin required** (`foo_mediacontrol`) since v1.5.1 | Title, Artist, Thumbnail, Stop; no shuffle/repeat/timeline | Prev/Next | Without plugin, no SMTC at all | Active niche |
| **MusicBee** | Local | Win32 | 🟡 **Plugin required** (`mb_MediaControl` ameer1234567890 / HenryPDT fork) | Full incl. shuffle/repeat/stop; timeline partial | Prev/Next | Best Win32 player-plugin combo for SMTC parity | Active niche |
| **Winamp** | Local | Win32 (2024 revival) | 🟡 **Plugin required** (`gen_smtc`, `gen_smtcinterop`, `gen_w10mc`) | Title, Artist, Thumbnail partial | Prev/Next | Native binary alone does **not** call SMTC despite the 2024 relaunch | Active niche |
| **VLC (Desktop)** | A/V | Win32 | 🟡 **Plugin** (`spmn/vlc-win10smtc`) for 3.0.x; **native `win_smtc` module is in mainline merge request `!3010`** | Title, Artist, Thumbnail partial | Prev/Next | Plugin DLL bitness must match VLC; expect "built-in" status to flip with VLC 4.0 | Mainstream major |
| **VLC (UWP)** | A/V | Microsoft Store | ✅ Built-in | Title, Artist, Thumbnail, App info, Stop | Prev/Next | Strong out-of-the-box | Active niche |
| **Windows Media Player / Media Player (Win 11)** | Local | Inbox | ✅ Built-in (Groove successor) | **Title and Artist often blank**, Thumbnail partial, Timeline present | **Prev/Next disabled** | Plays well but reports almost nothing about what's playing | Mainstream major (inbox) |
| **AIMP** | Local | Win32 | 🟡 **Plugin** (aimp.ru catalog id 1097) | Title, Artist, Thumbnail | Prev/Next | Plugin is officially blessed | Active niche |
| **MediaMonkey** | Local | Win32 | ✅ Built-in | Title, Artist, App Info; **no thumbnail**, no shuffle/repeat | Prev/Next | Missing album art is the standout quirk | Active niche |
| **iTunes** | Music | Win32 + Microsoft Store | 🟡 **Plugin required** (`thewizrd/iTunes-SMTC`) | Title, Artist, Thumbnail, Stop | Prev/Next | iTunes itself "locks" media keys preventing other apps from seeing them while running | Legacy mainstream |
| **DeaDBeeF** | Local | Win32 (port) | 🟡 **Plugin** (`DeaDBeeF-for-Windows/ddb_smtc`) | Title, Artist, App info; thumbnail partial | Prev/Next | Linux-origin player with Win build | Very niche |
| **MPV** | A/V | Win32 | 🟡 **Plugin** (MPVMediaControl by datasone, or MPV-SMTC by x0wllaar) | Title, Artist, Thumbnail (varies by plugin) | Prev/Next (varies) | Two different plugins with different field coverage | Active niche |
| **MPC-HC (clsid2)**, **MPC-BE** | A/V | Win32 | ✅ Built-in | Title, Artist; Thumbnail partial | Prev/Next | Maintained forks have SMTC | Active niche |
| **Dopamine, Rise Media Player, FeelUOwn** | Local | Win32/UWP | ✅ Built-in (full) | Full incl. shuffle/repeat/timeline | Prev/Next | High-quality community players | Very niche |
| **Audacious, Strawberry, Quod Libet, MusicPlayer2, QQ Music, PotPlayer, KMPlayer, GOM, Kodi-UWP** | Various | Win32 / UWP | ❌ No SMTC by any means | None | None | Hard-pass: cannot integrate without source patches | Various |
| **Audible (native Win)** | Audiobook | **Discontinued January 13, 2022; stopped working July 31, 2022** (Audible email quoted by Softpedia/gHacks) | N/A | N/A | N/A | Amazon Appstore-on-Windows-11 fallback unavailable for new installs since **March 6, 2024**, fully sunset **March 5, 2025** (Amazon developer bulletin) | Dead |
| **Audible Cloud Player** | Audiobook | Browser only | ✅ Likely via browser Media Session (matches Audible iOS lock-screen pattern; not directly verified) | Title, Author (in Artist), Thumbnail | Prev/Next maps to chapter (likely) | Identity = browser; lacks sleep timer & deep nav of the dead native app | Mainstream major |
| **Audiobookshelf (web)** | Audiobook | Self-hosted browser | ✅ **Confirmed Media Session API** (regular player); broken on the "share" player (GitHub #3768) | Title, Author (Artist), Cover | Prev/Next = chapter | Shared-link player has the bug; otherwise reliable | Active niche |
| **Libby / OverDrive** | Audiobook | Browser only (`libbyapp.com`) | ✅ Likely via Media Session (iOS lock-screen confirmed; web client SMTC inferred) | Title, Author, Cover (likely book title in Title field) | Prev/Next = chapter (likely; Libby exposes chapter UI and 15s skip) | **No public source confirms or denies playback-rate suffix in metadata** — verify by DOM inspection | Mainstream major (US libraries) |
| **Libro.fm** | Audiobook | Browser only | ✅ Likely (explicit prev/next/skip-15 controls in web player per Libro.fm blog) | Title, Author, Cover | Prev/Next + ±15s | Identity = browser | Active niche |
| **Speechify** | TTS / spoken | Native MSIX **launched March 31, 2026** (PRWeb / TechCrunch) + web app at app.speechify.com | Native: unverified; Web: likely via Media Session | Likely document/article title | Likely none meaningful | Newest entrant — public SMTC reports absent | Active niche |
| **iHeartRadio** | Radio | UWP Microsoft Store | ✅ Likely built-in (UWP `MediaPlayer` convention) | Title (station/show), Artist (now-playing track), Thumbnail | Limited (live radio) | Some now-playing slots refresh slowly | Mainstream (US) |
| **TuneIn Radio** | Radio | UWP Microsoft Store | ✅ Built-in | Title, Thumbnail, App info; Artist partial | **Prev/Next = disabled** (radio has no nav) | Useful for "now playing on station X" identification | Mainstream major |
| **BBC Sounds** | Radio / spoken | Browser only on Windows (per Microsoft Q&A reply citing BBC's own site) | ✅ Likely via Media Session | Programme title, Show, Cover | Prev/Next likely none meaningful | Identity = browser | Mainstream (UK) |
| **Radio.garden** | Radio | Browser only | ✅ via Media Session (limited fields) | Station name in Title | None | Geographic radio explorer | Very niche |
| **NPR One** | Spoken news | **Sunset December 13, 2023** (NPR official press release) | N/A | N/A | N/A | Folded into the new NPR app, which is iOS/Android only | Dead |
| **NPR (main app)** | Spoken news | iOS/Android only; web at npr.org | Browser session only on Windows | Browser-default | Browser-default | No Windows native | Mainstream |
| **Apple Podcasts** | Podcast | **None on Windows** | N/A | N/A | N/A | Not bundled in Apple Music for Windows | Mainstream |
| **Spotify (podcast playback)** | Podcast | Same Spotify app | ✅ Same session type as music | Episode in Title, Show in Artist | Prev/Next (episode-aware) | Single SMTC session whether playing music or podcasts — distinguish via metadata, not via separate session | Mainstream major |
| **Pocket Casts** | Podcast | Electron desktop (**v2.0 launched June 25, 2024**, per Pocket Casts Blog) + web | ✅ Media keys + lock-screen on legacy Win app (Ctrl.blog); Electron v2 retains media-key support; **Microsoft Store build temporarily unavailable** | Episode in Title, Show in Artist, Cover art | Prev/Next, ±skip | July 2024 forum posts report missing taskbar-thumbnail buttons after the v2 rewrite — media keys still route | Active niche / growing |
| **Overcast** | Podcast | None (iOS only) | N/A on Windows; overcast.fm lacks media keys (per Undercast premise) | N/A | N/A | Third-party Electron wrappers add SMTC externally | Mainstream (iOS) |
| **Podbean** | Podcast | Browser only | Browser session if any | Unverified | Unverified | Treat as generic browser tab | Active niche |
| **Brain.fm** | Ambient | Web only (web.brain.fm); third-party Electron wrappers | Browser session | Track/mode name in Title | None meaningful | Best as generic browser session | Active niche |
| **Calm** | Meditation | **No first-party Windows app** | N/A native | N/A | N/A | "Calm"-named Store apps are third-party clones | Mainstream (mobile) |
| **Headspace** | Meditation | **No first-party Windows app** (Headspace help: *"Yes, Headspace can be accessed in a web browser, but we do highly encourage members to access Headspace via the app"*) | Browser session via web only | N/A native | N/A native | Browser-only path on Windows | Mainstream (mobile) |
| **endel.io** | Ambient | UWP Microsoft Store | Likely SMTC (UWP convention) | Mode name in Title (likely) | None meaningful | Confirm by testing if special-casing | Active niche |
| **Noizio** | Ambient | None on Windows (mac/iOS only); community port `noizio.net` exists but is unofficial | N/A | N/A | N/A | "Noizio for Windows" listings are unofficial ports | Very niche |
| **Duolingo** | Language | UWP Microsoft Store | Unlikely persistent SMTC (short clip-based audio) | N/A meaningful | N/A | Treat as not present | Mainstream |
| **Pimsleur** | Language | Web only (`learn.pimsleur.com`) | Browser session only | Lesson title likely in Title | Likely none | Legacy Pimsleur Course Manager is a downloader, not a player | Active niche |

### Browser / PWA section (expanded)

- **Chromium hosts** emit SMTC sessions whose `SourceAppUserModelId` reports the browser, not the website. There is a single active media session at a time per browser process; switching the audible tab reassigns the existing session in place.
- **Firefox** behaves equivalently for play/pause/prev/next/thumbnail/title/artist but **does not supply timeline information** to SMTC (Mozilla Bugzilla 1689538). Internally Firefox creates an invisible window bound to `ISystemMediaTransportControls`; the timeline gap requires `ISystemMediaTransportControls2`, which Firefox has not historically implemented.
- **Web Media Session ⟶ SMTC mapping:** `MediaMetadata.title` → SMTC `Title`; `MediaMetadata.artist` → SMTC `Artist`; `MediaMetadata.album` → SMTC `AlbumTitle`; `MediaMetadata.artwork[0]` URL → fetched by the browser and inserted as the SMTC `Thumbnail` stream. So the SMTC thumbnail from a browser session is a **bitmap stream the browser already fetched and converted**, not a URL — your mod cannot bypass it to re-download a higher-resolution cover from the original site.
- **PWA-installed apps (Chrome / Edge "install as app")** still report under the parent browser's AUMID in every observed case on current Windows builds. There is no separate per-PWA SMTC source identity for app-windowed PWAs. Differentiating PWAs from regular tabs requires inspecting the `MediaMetadata` content (artist/album patterns), not the source.
- **Edge has a "Hardware media keys" setting** ("Let media play in this device control hardware media keys") that, if disabled, suppresses Edge from registering SMTC entirely.
- **Tor and Pale Moon do not register SMTC at all** (per the ModernFlyouts compatibility table).

### Priority tiers for a Windhawk taskbar media controller

**Tier 1 — high user volume AND quirks justifying special-casing:**
1. **Spotify (native)** — fields are all there; just trust them. Probably the highest-volume single source.
2. **Apple Music (native)** — full fields except shuffle/repeat; map "no shuffle/repeat" gracefully.
3. **Tidal (native)** — patch around missing `AlbumTitle` (Chromium issue 349310439); hide an empty Album line rather than printing "Unknown".
4. **Chromium browser sessions (Chrome + Edge + PWAs)** — biggest catch-all; expect one session per browser, no timeline, no shuffle/repeat. Treat source identity as "browser" and relabel based on metadata heuristics when confidence is high.
5. **Firefox** — same coverage as Chromium **minus timeline** — never show a position slider for Firefox sessions.
6. **Libby / Audiobookshelf / Libro.fm / Audible Cloud Player** — audiobook semantics: "Artist" is author/narrator, "Album" is series, prev/next means chapter, position can be multi-hour. UI should not assume "track ≈ 4 minutes". This is the *single biggest reason* audio-focused mods benefit from a dedicated audiobook code path.
7. **Pocket Casts (Electron v2.0 from June 25, 2024)** — episode in Title, show in Artist, prev/next means "previous/next episode in queue"; users want a custom skip-forward 30s / back 10s mapping not directly exposed by SMTC.
8. **Windows Media Player / Media Player (Win 11)** — known to leave Title/Artist blank for some local files; fall back to filename and don't assume metadata.
9. **iHeartRadio / TuneIn / BBC Sounds / Radio.garden** — radio sessions with no meaningful prev/next; hide prev/next buttons when both `IsPreviousEnabled` and `IsNextEnabled` are false.
10. **Amazon Music** — historically imperfect media-key handling; users may need explicit special-casing.

**Tier 2 — moderate audience or straightforward SMTC, default-handler is fine:**
- VLC UWP, MPC-HC (clsid2), MPC-BE, Movies & TV, myTube Beta, Dopamine, Rise Media Player, FeelUOwn, SoundCloud Store app, Crunchyroll UWP (play/pause only, but predictable), Deezer.

**Tier 3 — browser-only, niche, plugin-dependent, or no SMTC:**
- Plugin-dependent (advise user to install plugin or detect installed plugins): foobar2000, MusicBee, AIMP, Winamp, VLC desktop 3.x, iTunes, DeaDBeeF, MPV, Spicetify.
- Browser-only niche: Bandcamp, Nugs.net, Qobuz web, Pandora, Brain.fm, Headspace web, Podbean, Pimsleur web, Radio.garden.
- No SMTC by any means: Audacious, Strawberry, Quod Libet, MusicPlayer2, QQ Music, PotPlayer, KMPlayer, GOM Player, Kodi (UWP), Amazon Prime Video, FreeTube, Dailymotion.
- Dead / no Windows presence: Audible native (discontinued January 13, 2022), NPR One (sunset December 13, 2023), Apple Podcasts (never), Overcast (iOS only), official Noizio (mac/iOS), official Calm/Headspace (mobile/web only), official Brain.fm (web only).

### Gaps & unknowns (verify before shipping)
- **Libby's exact field mapping** — Title = chapter or book title? Artist = author or "narrated by …"? Does a speed suffix ever appear? Reverse-engineer in DevTools (`navigator.mediaSession.metadata`).
- **Speechify Windows native SMTC behavior** — too new (March 31, 2026 launch).
- **Qobuz native client** field coverage — not in any reputable comparison list found.
- **Endel UWP** SMTC field coverage — likely auto-integrated but no community confirmation.
- **Amazon Music** field-by-field SMTC coverage and whether it correctly sets `MediaPlaybackType.Music`.
- **Apple Music radio** — whether prev/next is disabled on Apple Music radio stations is unverified.
- **PWA per-origin SMTC identity** — confirm on latest Edge / Chrome whether app-windowed PWAs ever get distinct AUMIDs in Windows 11 24H2+.
- **Brain.fm Windows presence** — confirm whether their "desktop app" link is first-party Electron or just a PWA wrapper.

## Recommendations

**Stage 1 — Default-handler that "just works" for 80% of users (~1–2 weeks):**
Implement standard `GlobalSystemMediaTransportControlsSessionManager` polling. For each session, read Title / Artist / Album / Thumbnail (as a `RandomAccessStreamReference`, render to a bitmap) / Timeline if present / `IsXxxEnabled` flags. Display buttons only when their enabled flag is true. This covers Spotify, Apple Music, Tidal, YouTube Music PWA, VLC UWP, iHeartRadio, TuneIn, and most native UWP apps adequately. **Benchmark to advance:** at least 95% of test sessions render Title and Artist correctly with the default handler.

**Stage 2 — Tier-1 special-casing (~1 week):**
1. **Source-identity normalization** — recognize `Chrome.exe`, `msedge.exe`, `firefox.exe`, `brave.exe`, `opera.exe` and label them generically as "Browser"; then look at `MediaMetadata.artist` / Title patterns to relabel as YouTube Music, Spotify Web, Bandcamp, etc.
2. **Audiobook detection** — if `Position` > ~1 hour OR `EndTime` > ~1 hour OR title contains "Chapter NN" / "Part NN", switch UI to audiobook mode (relabel "Artist" → "Author", "Prev/Next" → "Prev/Next chapter", show HH:MM:SS position).
3. **Tidal album workaround** — when source = Tidal and `AlbumTitle` is empty, suppress the Album row rather than showing "Unknown".
4. **Firefox timeline suppression** — detect Firefox source and hide the position scrubber entirely.
5. **No-nav radio mode** — when both `IsPreviousEnabled` and `IsNextEnabled` are false but playback is live, switch to a "Stop / Listen Live" UI rather than dimming nonexistent buttons.
6. **MediaMonkey thumbnail fallback** — when source = MediaMonkey, prefer a query against the local audio file tag (if path is exposed) over the absent SMTC thumbnail.

**Stage 3 — Plugin-aware messaging (~few days):**
Detect installed plugins (`foo_mediacontrol`, `mb_MediaControl`, AIMP `rec_id=1097`, `gen_smtc`, `vlc-win10smtc`, `iTunes-SMTC`, `ddb_smtc`, MPVMediaControl). When the user runs foobar2000 / MusicBee / AIMP / Winamp / VLC-desktop / iTunes / DeaDBeeF / MPV without a plugin installed, offer a one-click link to the documented GitHub release.

**Stage 4 — Decision benchmarks for adding or dropping further special cases:**
- Add a special case if any single app accounts for ≥ 5% of user-reported mismatches in your telemetry.
- Drop a special case once ModernFlyouts-Community / FluentFlyout updates its compatibility table to "Built-in: full" (e.g., once VLC 4.0 ships with built-in `win_smtc`, remove the VLC plugin nag).
- Re-test annually around major release cycles: Apple Music for Windows ships notable SMTC changes (April 25, 2023 was the keyboard/media-key milestone), and Pocket Casts shipped its Electron rewrite in June 2024.

## Caveats

- The most authoritative open compatibility table is the **ModernFlyouts-Community/ModernFlyouts** `GSMTC-Support-And-Popular-Apps.md` document; the repo was archived November 15, 2025 and is now read-only. **FluentFlyout** (a successor) maintains an abbreviated list. Both are community-maintained and may lag app updates.
- **"Built-in" status can regress between versions** — foobar2000 ships SMTC via component since v1.5.1 but installing without the component yields no SMTC; Pocket Casts' Electron v2 (June 25, 2024) regressed taskbar thumbnail buttons relative to the legacy app per forum reports.
- **Browser SMTC behavior depends on user-controllable flags** ("Hardware media keys handling", "Global media controls"); these can suppress SMTC entirely without your code being aware.
- **Audiobook / podcast field semantics are not standardized.** The same SMTC fields carry book title / author / series — or — episode title / show name / network — depending on app. Heuristic content classification is required if your UI distinguishes them.
- **"Likely" vs. "confirmed" flags throughout this report** reflect source availability. Items flagged "likely" (Libby, Libro.fm, Audible Cloud Player on desktop browsers, BBC Sounds, Endel UWP, iHeartRadio UWP) should be verified by DOM/DevTools or by running `GetCurrentSession()` in a test harness before being relied on for shipping logic.
- **The Windhawk environment runs in-process with explorer.exe.** Consuming `GlobalSystemMediaTransportControlsSessionManager` from explorer.exe is supported (Windows itself does so for the inbox media flyout widget), but be cautious about COM apartment threading — the existing ModernFlyouts / FluentFlyout codebases and the `DubyaDude/WindowsMediaController` NuGet wrapper are good references.
- **No source documents the exact "playback-rate suffix in metadata" behavior** mentioned in the task — neither for Libby nor any other app surveyed. Treat this as an unverified user-report observation; test by playing a Libby title at 1.5× / 2× speed and inspecting `GlobalSystemMediaTransportControlsSessionMediaProperties.Title` directly. If confirmed, that would be a Libby-specific quirk worth stripping in the mod's display layer.