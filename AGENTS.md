# Native Taskbar Media Controller

A Windhawk mod (v1.5.0) that injects a native media controller into the Windows 11 taskbar's own XAML tree: now-playing text and art from any GSMTC media session, playback controls, a hover flyout, multi-session cycling and an audiobook mode. The whole mod is one source file. It is a public GitHub repository (StarlightDaemon/Native-Taskbar-Media-Controller), and the mod is listed in Windhawk's mod catalog.

## Verifying changes

There's no build script or test suite. The mod is compiled by Windhawk when it's loaded (Windhawk → Develop → Load mod from file).
- Loading, reloading or installing the mod changes the running taskbar, so it's the owner's step, not an agent's. Don't load or install it unless the owner asks.
- Without loading, verify by careful review of `native-taskbar-media-controller.wh.cpp`: the mod metadata and settings blocks at the top, version strings kept in sync (the `@version` line, `README.md`, `.raiden/state/CURRENT_STATE.md`, `.raiden/state/GOALS.md`), and the teardown rules below.

## Layout

- `native-taskbar-media-controller.wh.cpp`: the mod (metadata, readme and settings blocks, then the code)
- `README.md`: user documentation and compatibility table
- `docs/`: `audit-reports/`, `fork-reports/` (reviews of related community mods), `prompts/` (past agent prompts)
- `.raiden/state/`: the project records (see below)

## Key rules

- Main branch is `main`. Agent work goes on `<agent>/<task>` branches in their own worktrees. Ask the owner before pushing, tagging, or submitting an update to the Windhawk mod catalog.
- Teardown must stay complete: every event token revoked, every thread joined and all XAML work done on the UI thread before `Wh_ModUninit` returns (see the v1.3.0 and v1.5.0 notes in `CURRENT_STATE.md`). Hot unload must never leave callbacks running.
- `forks/` (local clones of other people's mods) and `.audits/` are gitignored; never commit them.
- No `Co-Authored-By` or other agent attribution lines in commits; the `commit-msg` hook enforces this. Never bypass it.
- Don't edit `.raiden/writ/` or `.raiden/instance/`. They're historical RAIDEN v2 text kept for the old tooling. Where they, `.raiden/README.md` or `.raiden/local/` disagree with this file, this file wins.

## Project records

This file and `.raiden/state/` are authoritative for this repo. Read in this order and stop when you have what you need:
1. `.raiden/state/CURRENT_STATE.md`: the current release at the top. It's about 10 KB of stacked release notes; only the first section is current (OL-12).
2. The `## Active` section at the top of `.raiden/state/OPEN_LOOPS.md`: open, deferred and held items (statuses: `open`, `deferred`, `held: <who>`, `no-action`, `accepted`, `closed`).
3. Only when needed: `DECISIONS.md`, `WORK_LOG.md` and `docs/audit-reports/` (history and evidence). `AUDIT_LOG.md`, `FORK_REVIEW_LOG.md` and the `last-*.md` files in `.raiden/state/` are gitignored, so they exist only in the main checkout.

Current picture: one open item, OL-12. It is ordinary work: trim `CURRENT_STATE.md` to the current release and move per-release history to `WORK_LOG.md`. Everything else is closed.

Update `CURRENT_STATE.md` and `OPEN_LOOPS.md` when you finish or pause work. Keep one home per fact and link evidence; don't paste it in. Never renumber or reuse a loop or decision ID. Medium+ audit findings become loops.

## Working in parallel

One agent per worktree. Before starting, read the Claims section of `../../HANDOFF.md` (the workspace folder two levels above the repo or worktree root, outside Git, so every worktree sees the same file) and `git worktree list`, then add your claim line there; mark it done when you finish. Separate tasks may run in parallel only when both claims are listed there and they don't touch the same files. The owner (or the agent they assign) merges branches and updates `.raiden/state/`. Releases, catalog submissions, the hook and Git config are one-at-a-time work.

This repository is public: don't commit personal names, absolute local paths or details of the owner's private tooling.
