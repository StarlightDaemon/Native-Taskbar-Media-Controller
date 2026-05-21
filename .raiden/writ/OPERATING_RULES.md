# Writ Operating Rules

This file is a RAIDEN-managed law artifact inside an installed Writ.

## Core Rules

1. Treat files in `.raiden/writ/` as RAIDEN-managed core.
2. Put repo-specific adaptation in `.raiden/local/`, not in managed-core files.
3. Put continuity and fast-changing operational truth in `.raiden/state/`.
4. Treat local edits to managed-core files as exceptions that require explicit
   update conflict handling rather than silent overwrite.
5. Review RAIDEN updates before applying to understand scope of changes.

## Commit Attribution Policy

Commits in a RAIDEN Instance must carry only the operator's git identity.

- Do not add `Co-Authored-By`, `Co-authored-by`, or any agent attribution
  trailer lines to commit messages.
- The `commit-msg` hook installed by RAIDEN enforces this at the git level.
  Do not remove or bypass it.

## Branch Naming

The mainline branch in every RAIDEN Instance must be named `main`.

- If `git init` created a `master` branch, rename it before the first RAIDEN
  install commit: `git branch -m master main`
- Do not create or leave a `master` branch as the default.
