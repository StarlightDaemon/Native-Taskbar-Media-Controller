# Repository Audit — native-taskbar-media-controller

**Audit date:** 2026-06-24 · **Commit audited:** `13f0daf` (branch `main`) · **Instrument:** Repository Audit v4.3 (read-only) · **Host:** macOS (Darwin 25.5.0)

---

## 0. Executive summary

**Identity.** A single-file [Windhawk](https://windhawk.net) C++ mod that injects a native media controller into the Windows 11 taskbar's own XAML tree (`Native Taskbar Media Controller`, `@id native-taskbar-media-controller`, author *StarlightDaemon*, MIT). Now-playing info + playback controls sourced from Windows GSMTC sessions.

**Stack composition.** Single ecosystem: **C++ / WinRT (Windhawk mod)** — one 3,092-line translation unit (`native-taskbar-media-controller.wh.cpp`) compiled by the Windhawk loader via header `@compilerOptions`. **No** package manager, manifest, lockfile, CI/CD, IaC, or env files. The substance beyond the mod is a heavy `.raiden/` governance/state layer (28 files) and a `docs/` tree (audit reports, fork reviews, prompts). A gitignored `forks/` directory (~1 MB, ~13 community fork variants) is local reference material, not redistributed.

**Maturity stage.** Mature/active solo project. 67 commits (first 2026-05-21, last 2026-06-12 → **not dormant**, ~12 days before audit). Effectively single-author (StarlightDaemon; 4 governance commits under a placeholder `[redacted email]`). Tagged releases through `v1.4.9`; source already at `v1.5.0` (unreleased).

**Repository size.** 76 working-tree files (excl. `.git`/`node_modules`) → **full audit, not sampled**.

**Finding counts by severity:** Critical **0** · High **0** · Medium **8** · Low **12** · Info **15**.
*(Both candidate High findings — the v1.4.8 frozen state doc and the untagged v1.5.0 source — were downgraded to Medium by adversarial verification: real and confirmed, but documentation / release-bookkeeping drift with no runtime, security, or build impact.)*

**Dominant theme.** One root cause produces most Medium findings: the source advanced to **v1.5.0 plus two shipped features**, but the README, the `.raiden` state ledger, the git tags, and the settings docs were **never updated** — leaving four disagreeing version strings and an undocumented feature. See **F1**.

### Provenance table (what was and was not verified)

| Check | Status | Detail |
|---|---|---|
| Working-tree secret sweep (filesystem walk, incl. `forks/`) | ✅ verified | Zero real secrets. All mod hits are WinRT `event_token` false positives; high-signal patterns (AKIA/`sk-ant-`/`ghp_`/`xox`/`AIza`/JWT) and credential files (`*.pem`/`id_rsa`/`*.keystore`/`credentials.json`) all zero. |
| Git-history secret scan | ✅ verified | `gitleaks` scanned **68 commits** (~3 MB) — **no leaks found**. (Tooling present; not the not-verified ❌ fallback tier.) |
| Env-file leak classification | ✅ verified | No `.env`/`.env.*` files exist anywhere. |
| Config completeness (settings declared vs read) | ✅ verified | 10 declared settings = 10 read; zero stale, zero undocumented. |
| Per-ecosystem SCA / outdated deps | ⚪ not-applicable | No manifest/lockfile exists; SCA is *structurally* inapplicable to a Windhawk single-file mod (only Windows system libs linked). Stated explicitly, not "N/A". |
| Test execution | ❌ not-verified | Windows-only mod (WinRT XAML, `Taskbar.View.dll` symbol hooks, `@architecture x86-64`); cannot build/link/test on this macOS host. `timeout`/`gtimeout` also absent. No suite exists. |
| Static analysis / compile | ❌ not-verified | Same reason — Windows-only toolchain; read-only static review only. |
| CI status | ⚪ not-applicable | No CI/CD config (`.github/workflows`, GitLab, Circle, Azure, Jenkins) present; `gh run list` empty. No green/red proxy available. |
| IaC misconfiguration scan | ⚪ not-applicable | No Terraform/K8s/Helm/Dockerfile/Ansible/CFN in the repo. |
| Remote/tag state | 🟡 partial | Live tag state verified via `git ls-remote --tags origin` and `git branch -a --contains`; full `git fetch --all --prune` **not run** — `main…origin/main [ahead 1]` is from the local tracking ref (point-in-time, **Volatile**). |
| Licensing | ✅ verified | Root MIT `LICENSE` (StarlightDaemon, 2026) present and referenced by README. |
| Declared-state reconciliation (`.raiden`) | ✅ verified | All 28 `.raiden` files enumerated; state/* read in full; reconciled against git + code. |
| Untracked-file classification | ✅ verified | `git status -uall` + `git check-ignore -v` + `git ls-files` per path. |

---

## 1. Identity

- **What it is.** A Windhawk mod that inserts a media-controller widget directly into `Grid#RootGrid` under `Taskbar.TaskbarFrame` — no overlay window, no `SetLayeredWindowAttributes`, no GDI paint loop; the widget inherits z-order, auto-hide, and DPI scaling from the taskbar.
- **Who it's for.** Windows 11 (22H2+) users running the Windhawk mod loader who want native now-playing/playback controls in the taskbar.
- **Language/ecosystem.** C++ with WinRT/XAML; Win32/COM/GDI/DWM interop. Single `.wh.cpp` translation unit.
- **Entry points.** Windhawk lifecycle hooks: `Wh_ModInit` / `Wh_ModSettingsChanged` / `Wh_ModUninit` (`native-taskbar-media-controller.wh.cpp:2969`+), plus injection driven by hooks on `Taskbar.View.dll`.
- **How it runs/builds.** Compiled in-place by Windhawk on a Windows 11 host via header `@compilerOptions` (`-lole32 -loleaut32 -lruntimeobject -luser32 -lwindowsapp -lshell32 -lgdi32 -ldwmapi -DWINVER=0x0A00` + 4 `-Wl,--undefined` directives). No external build system; cannot be built on this macOS host.

---

## 2. Current state

- **Source version:** `@version 1.5.0` (`native-taskbar-media-controller.wh.cpp:5`) — **unreleased / untagged**.
- **Latest tag:** `v1.4.9` (commit `e62104b`, 2026-05-26), pushed to `origin/main`. `v1.4.8` also tagged + pushed.
- **Branch:** `main` at `13f0daf`, **ahead of `origin/main` by 1** (Volatile — see §3). HEAD is 14 commits past `v1.4.9`, including two **feature** commits (`97096a7` widget-position setting; `ffd5ba7` dynamic flyout font sizing + the 1.5.0 bump).
- **CI/CD pipeline health:** none configured. For a Windhawk mod (Windows-only, loader-compiled) this is expected; noted as Info, not a defect. No remote pipeline to disagree with the local tree.
- **What works:** Feature claims spot-checked against code and confirmed present and wired — marquee scroll, hover flyout, audiobook mode, multi-session chip, `MiddleButtonPressed` stop, `DoubleTapped` focus, `WidgetPosition` (`enum class WidgetPosition { Right, Left, Center }`, ~14 usages).

**Declared-state reconciliation (`.raiden` governance layer).** The live-state ledger is **materially stale** relative to git and code:
- `CURRENT_STATE.md` still frames the project at **v1.4.8 "committed, awaiting tag + push"** with "latest release tag v1.4.7" — contradicted by git (v1.4.8 **and** v1.4.9 both tagged and pushed; source already at v1.5.0).
- `OPEN_LOOPS.md`: **every** loop (OL-2…OL-11 + SC-UI-3) is struck-through/CLOSED with cited commits — **no open declared loops remain**.
- Four `HANDOFF-*.md` briefs all describe shipped/closed work (one, `HANDOFF-OL9`, documents the *Chameleon* background subsystem since **removed**).
- `DECISIONS.md` is an empty stub despite real scope decisions living in narrative docs.

---

## 3. Git state & history

> **Volatile** — git state is point-in-time. Re-verify the items below before acting; a full `git fetch --all --prune` was **not** run this audit.

- **Working tree:** clean (no staged/uncommitted changes to tracked files; **no stashes**).
- **Unpushed:** `main` is **1 commit ahead** of `origin/main` (`13f0daf chore: update RAIDEN Instance to Edict v1.0.0`). *(Volatile)*
- **Branches:** `main` and `feature/rename-to-native-controller` (local `ea0f0ce` + `origin/feature/...`). The feature branch is **fully merged into main** (`git rev-list main..feature/... = 0`; main 59 ahead) yet its refs were never pruned, and its tip still declares `@version 0.1.0-beta.1` — a divergent version scheme. Closure already declared in OL-3. *(Volatile — see F5.)*
- **Cadence / dormancy:** 67 commits over 2026-05-21 → 2026-06-12; **active**, not dormant (Info context — does not recolor other findings).
- **Contributors:** effectively single-author — StarlightDaemon (67); 4 governance commits authored as `[redacted identity]` (placeholder email — minor oddity, F-Info).

**Untracked / ignored file classification:**

| Path | Bucket | Note |
|---|---|---|
| `.audits/` | **Tool directory** | This audit's own output. Not flagged. Recommend gitignoring (§6). |
| `.serena/` (only `.serena/.gitignore`, `.serena/project.yml` untracked) | **Tool directory** | Serena LSP-agent config; `cache`/`project.local.yml` self-ignored; `memories/` empty; template unmodified, no secrets. |
| `.claude/settings.local.json` | **Tool directory** | Ignored by the **global** excludesfile only, not repo `.gitignore` (see F4). |
| `.raiden/local/MODEL_MAP.md` | **Tool directory** | RAIDEN local overlay (model ids only; self-labeled "never commit"). |
| `forks/` | **Tool directory** (reference) | Gitignored, 0 tracked files; ~13 fork variants + `MANIFEST.md`. |
| `.DS_Store` | **Orphaned artifact** | macOS Finder metadata; double-ignored (repo + global). Harmless. |
| `check_xaml.cpp` | **Orphaned artifact** | 16-line throwaway WinRT compile-probe; both probe calls commented out; mtime predates last mod edit (F12). |

---

## 4. Open loops

**Declared ledger (`.raiden/state/OPEN_LOOPS.md`):** all loops CLOSED with cited commits — OL-2 (Phase-2 features, `b58aecb`), OL-3 (rename branch merge, `dc4c0d2`), OL-4/OL-5 (TOCTOU + drain-timeout hardening), OL-6 (Libby, `3f2f5a5`), OL-7 (cover art, `bf5b4c9`), OL-8 (cold-boot fix, `979e033`), OL-9 (background theming), OL-10 (marquee live test, deferred), OL-11 (WSL→macOS migration, `7a91ec3`), SC-UI-3 (title truncation). **No open declared loops.**

**Discovered, untracked by the ledger:**
- **Two shipped v1.5.0 features** (`WidgetPosition`; dynamic flyout font sizing) appear in **no** state doc — the ledger ends at v1.4.8 (F1b).
- **README introduces roadmap IDs** `SC-HT-5` (per-app volume) and `SC-HT-3` (shuffle/repeat) that exist in **no** ledger (F3b).
- **README/state scope contradiction:** README lists `SC-0X-1` (display-only mode) "under consideration"; `CURRENT_STATE.md:82` marks it "**NOT WANTED — will not be implemented**" (F3a).

**Declared-but-resolved (stale pointers):** `last-audit.md` priority "tag and push v1.4.8" is already done; `AUDIT_LOG` newest entry (2026-05-25) predates 16 later commits (F1f). Four `HANDOFF-*.md` linger as if in-flight though all map to CLOSED loops (Info).

In-source discovery: **zero** `TODO`/`FIXME`/`HACK`/`XXX`/`BUG` markers and **zero** commented-out dead-code blocks in the mod (grep clean).

---

## 5. Code quality & structure

**Architecture.** Unusually well-organized for a single 3,092-line file: clearly delineated sections (Settings, GSMTC multi-session state, XAML injection, flyout, helpers, widget construction, marquee, fullscreen polling, hooks, entry points), named-constant element keys (no stringly-typed lookups), a shared `WH_CATCH`/`WH_TRY_OR` exception macro, and RAII guards for async/hook ref-counting. Error handling around WinRT is pervasive and disciplined (41 `try` blocks, 21 `WH_CATCH`, 25 `g_Unloading` guard checks); the native vtable-scan path is memory-validated before any `QueryInterface`.

**Test coverage state.** No tests exist and none can run (Windows-only mod). **Test confidence: not measurable** — there is no suite to label; downstream verification depends on a Windows host. CI-green proxy unavailable (no CI).

**Config completeness (verified clean).** All 10 settings declared in `==WindhawkModSettings==` are read in `LoadSettings` (lines 206–226) and all 10 reads are declared — **zero** stale, **zero** undocumented config keys.

**Debt hotspots (all Localized unless noted):**
- **Incomplete unload teardown** (root cause group, F2): the mod claims clean unload and is mostly there, but (a) `g_TrayResizeToken` (tray `SizeChanged` subscription) is never revoked on uninit — only on re-injection (F2a); (b) two detached `std::thread` polling loops (`TriggerInitialScan`, `PollForTaskbarViewDll`) are never joined — they check `g_Unloading` so exit within ~100 ms, but `Wh_ModUninit` can return mid-`Sleep` (F2b).
- **Two ~340-line functions** concentrate most UI logic: `ApplyStateToWidget` (`:1847`, ~338 lines, incl. an embedded ~70-line `fire_and_forget` art-decode coroutine) and `BuildWidget` (`:1307`, ~340 lines, mostly linear) (F6).
- **High magic-number density** (~519 numeric literals), concentrated in GDI flyout drawing (`FlyoutWndProc :1105–1270`: DWM attr ids 33/20, RGB tuples, point sizes 13/11/9, alpha 235/255) and layout constants — inconsistent with the named-constant pattern used elsewhere (F7).
- **44 mutable globals** form the entire application state (345 references) with no encapsulation; cross-thread invariants (which of 4 mutexes guards which global) live only in comments. Locking discipline is currently careful and consistent, so this is maintainability debt, not a present bug (F8 — *Cross-cutting, Sprawling*).

**Doc state.** README and the in-mod `==WindhawkModReadme==` feature/compatibility tables are verbatim-identical; the README **Settings** table omits `WidgetPosition` and renames/under-describes `OffsetX` (F1e); README's "live repositioning" claim is unqualified though the code path is Right-mode-only (`:770`, F9). `docs/` has no index and carries orphaned historical prompt/handoff material (F10); a two-report self-audit lineage lives in `docs/audit-reports/` (Info).

---

## 6. Security & compliance

**Secrets — working tree:** ✅ clean. Full filesystem-walk grep (incl. `forks/`) found only WinRT `event_token` false positives. High-signal pattern sweep and credential-file search both zero. The one credential-shaped hit — a Musixmatch `user_token` in `forks/hibrittofas/.../taskbar-media-bar.wh.cpp:1125` — is an **empty static populated at runtime** from an HTTP fetch, not a committed secret (value never hardcoded; redacted regardless).

**Secrets — git history:** ✅ clean. `gitleaks` over **68 commits** found no leaks. (Required ❌-able check — passed at the verified tier, not the manual-fallback tier.)

**Env files:** none exist → Info (no leak surface).

**Config completeness:** no gaps — see §5 (10/10 settings reconciled).

**IaC:** not applicable — no infrastructure-as-code in the repo.

**Licensing & third-party compliance:**
- Repo's own license: **MIT**, Copyright (c) 2026 StarlightDaemon (`LICENSE`), referenced by `README.md:99` → complete and self-consistent (Info).
- `forks/` (~13 community fork variants, ~1 MB) is **correctly gitignored and fully untracked** (`git ls-files forks` = 0); `.gitignore` annotates it "reference material kept locally, **not redistributed**." Each fork carries its own `@author`/`@github` header; one declares `@license MIT`, one credits "Original by Hashah2311"; `forks/MANIFEST.md` tabulates provenance. Because the snapshots are excluded from the published repo, the distribution-side third-party-license obligation is not triggered. License compatibility **deliberately not adjudicated** (Info).
- `docs/fork-reports/` (12 tracked `.md`) are **prose reviews about** the forks, not source redistribution (only one embeds a single ~15-line illustrative snippet) → minimal concern (Info).
- The repo's own mod header asserts sole authorship with **no derivative/attribution note**, while ~13 related-mod forks are kept as design reference and reviewed in committed docs. No code-level copying was adjudicated (out of scope); surfaced only as provenance-hygiene (F11).

> **Info (self-output):** This report is written to `.audits/` inside the repo and will appear as an untracked file. `.audits/` is classified as a **Tool directory** (not orphaned). Recommend the operator add `.audits/` to `.gitignore` if these reports should not be tracked. *(The audit does not edit `.gitignore` itself.)*

---

## 7. Dependencies & tooling

- **Manifests/lockfiles:** **none** (no `package.json`/`go.mod`/`Cargo.toml`/`requirements.txt`/`vcpkg.json`/`CMakeLists.txt`). SCA and outdated-dependency analysis are **structurally inapplicable** to a Windhawk single-file mod — no package manager has any input to consume (Info).
- **Runtime/link deps:** all **Windows system libraries** linked via `@compilerOptions` (ole32, oleaut32, runtimeobject, user32, windowsapp, shell32, gdi32, dwmapi) — first-party OS DLL import libs, not version-pinned third-party packages; no CVE-tracking surface in the project-dependency sense (Info).
- **Shadow / OS-level deps & build prerequisites:** the only build path is **Windhawk + a Windows 11 host**; not buildable/linkable/testable on macOS (Info — structural environment fact, not a defect).
- **Lockfile health:** n/a (no lockfile).

---

## 8. Oddities

- **Four disagreeing version strings** for one checkout: code `1.5.0`, README `v1.4.9`, state `1.4.8`, claimed-latest-tag `v1.4.7` (actual latest `v1.4.9`) → F1.
- **Ignore strategy split** (F4): `.claude/settings.local.json` and the effective `.DS_Store` rule depend on the *user's global* `~/.gitignore_global`, not the repo `.gitignore`. A fresh clone on a machine without that global file would see `.claude/settings.local.json` as untracked — a clone-portability gap (file inspected, clean).
- **Stale merged branch** `feature/rename-to-native-controller` (local + remote) never pruned, with a divergent `v0.1.0-beta.1` header → F5 (Volatile).
- **Empty `DECISIONS.md` stub** despite real scope decisions scattered in narrative docs → F3c.
- **Lingering HANDOFF briefs** describing shipped/closed work (one documents a since-removed feature) → Info.
- **Placeholder commit author** `[redacted identity]` on 4 governance commits → Info.
- **Orphaned `check_xaml.cpp`** diagnostic probe (both probe calls commented out; predates last mod edit) → F12.
- **GitHub URL casing** in state docs uses `Native-Taskbar-Media-Controller` (matches the live remote, but inconsistent with the lowercased local dir / rename intent) → Info.

---

## 9. Findings index

Severity ∈ {Critical, High, Medium, Low, Info}; Effort ∈ {Trivial, Bounded, Sprawling}; Blast ∈ {Localized, Cross-cutting}. Root-cause groups use parent/sub IDs. Evidence, in-repo source-of-truth citations, and affected files live in the section bodies above. *(Cross-ref column reserved for tracked-loop IDs assigned during planning — none minted here.)*

| ID | Severity | Effort | Blast | Location | Finding | Cross-ref |
|---|---|---|---|---|---|---|
| **F1** | Medium | — | Cross-cutting | (parent) | **Release/state-doc drift:** source advanced to v1.5.0 + 2 features; tags, README, `.raiden` ledger & settings docs never updated | — |
| F1a | Medium | Trivial | Localized | `.raiden/state/CURRENT_STATE.md:14,16,19` | State doc frozen at v1.4.8 "pending tag+push"; claims latest tag v1.4.7 — both v1.4.8 & v1.4.9 actually tagged+pushed *(was High → Medium on verify)* | — |
| F1b | Medium | Bounded | Localized | `native-taskbar-media-controller.wh.cpp:5`; commits `97096a7`,`ffd5ba7` | Source `@version 1.5.0` untagged; 2 shipped features unrecorded in any state doc *(was High → Medium on verify)* | — |
| F1c | Medium | Trivial | Localized | `README.md:5` | README "Status: v1.4.9" lags code (1.5.0), leads state (1.4.8) | — |
| F1d | Medium | Trivial | Localized | `.raiden/state/GOALS.md:16` | Phase table repeats stale "latest tag v1.4.7, v1.4.8 pending" | — |
| F1e | Medium | Trivial | Localized | `README.md:51-62` | Settings table omits `WidgetPosition` (a v1.5.0 feature) & under-describes `OffsetX` | — |
| F1f | Low | Trivial | Localized | `.raiden/state/last-audit.md:11`; `AUDIT_LOG.md` | Stale audit pointer "tag and push v1.4.8" (already done); audit log not refreshed | — |
| **F2** | Medium | — | Localized | (parent) | **Incomplete unload teardown** — clean-unload claim has two residual leaks | — |
| F2a | Medium | Trivial | Localized | `native-taskbar-media-controller.wh.cpp:2255,2302,2969` | `g_TrayResizeToken` not revoked on uninit (only on re-injection) | — |
| F2b | Medium | Bounded | Localized | `native-taskbar-media-controller.wh.cpp:2847,2886` | Two detached `std::thread`s never joined; `Wh_ModUninit` may return mid-`Sleep` | — |
| **F3** | Medium | — | Localized | (parent) | **Roadmap ⇄ scope-ledger inconsistency** (README vs `.raiden`) | — |
| F3a | Medium | Trivial | Localized | `README.md:73` vs `CURRENT_STATE.md:82` | README lists SC-0X-1 "under consideration"; state marks it "NOT WANTED" | — |
| F3b | Low | Bounded | Localized | `README.md:71-72` | Roadmap IDs SC-HT-5, SC-HT-3 exist in no state ledger (untracked backlog) | — |
| F3c | Low | Bounded | Localized | `.raiden/state/DECISIONS.md` | Empty stub despite real scope decisions in narrative docs | — |
| F4 | Low | Trivial | Cross-cutting | `.gitignore` vs `~/.gitignore_global` | `.claude` (and `.DS_Store` mechanism) ignored only by user global excludesfile → clone-portability gap | — |
| F5 | Low | Trivial | Localized | branches `feature/rename-to-native-controller` (local+remote) | **Volatile** — fully merged but unpruned; divergent `v0.1.0-beta.1` header | — |
| F6 | Low | Bounded | Localized | `native-taskbar-media-controller.wh.cpp:1847,1307` | Two ~340-line functions concentrate UI logic (embedded art-decode coroutine) | — |
| F7 | Low | Bounded | Localized | `native-taskbar-media-controller.wh.cpp:1105-1270` | High magic-number density in GDI flyout drawing / layout | — |
| F8 | Low | Sprawling | Cross-cutting | `native-taskbar-media-controller.wh.cpp:660-772` | 44 mutable globals = entire app state, no encapsulation (maintainability debt) | — |
| F9 | Low | Trivial | Localized | `README.md:20`; `...wh.cpp:770` | "Live repositioning" claim unqualified (code path Right-mode only) | — |
| F10 | Low | Bounded | Localized | `docs/prompts/`, `docs/` | Orphaned historical prompts; no docs index | — |
| F11 | Low | Trivial | Localized | `native-taskbar-media-controller.wh.cpp:1-9` | Own mod header carries no derivative/attribution note despite fork references | — |
| F12 | Low | Trivial | Localized | `check_xaml.cpp` | Orphaned diagnostic probe (both probe calls commented out; ignored) | — |
| F13 | Info | Trivial | Localized | `.raiden/state/HANDOFF-*.md` (×4) | Completed handoffs linger as if in-flight (one documents a removed feature) | — |
| F14 | Info | Trivial | Localized | 4 commits | Placeholder commit author `[redacted identity]` | — |
| F15 | Info | Trivial | Localized | `CURRENT_STATE.md:15`, `WORK_LOG.md:124` | GitHub URL casing inconsistent with lowercased local dir (URL itself valid) | — |
| F16 | Info | Trivial | Localized | repo-wide | No CI/CD configured (expected for a Windhawk mod; no remote-pipeline signal) | — |
| F17 | Info | — | — | repo-wide | **Positive confirmations:** history+working-tree secret-clean; MIT license complete; `forks/` correctly untracked/non-redistributed; config 10/10 reconciled; no TODO/dead-code | — |

---

*End of report. Read-only audit — no repository contents, history, dependencies, or environment were modified. The single file created is this report under `.audits/`.*
