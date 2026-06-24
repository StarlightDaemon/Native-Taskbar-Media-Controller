# Audit Log

This file is append-only. Each entry is prepended to the top of the entries section by the audit agent. Do not edit entries retroactively.

---

## 2026-06-24 (commit audited: 13f0daf; HEAD after remediation: 59e2a37, branch main)

- Findings: Critical 0 | High 0 | Medium 8 | Low 12 | Info 15
- Dominant theme: release and state-doc drift (F1 group) — source
  advanced to v1.5.0 with two features untagged and all state docs
  frozen at v1.4.8; root cause for six of the eight Medium findings
- Completed this cycle: F2a (g_TrayResizeToken uninit revocation,
  aaf3e4a) and F2b (detached thread joins in Wh_ModUninit, 59e2a37);
  v1.5.0 tagged at 59e2a37
- Report: .audits/audit-2026-06-24-13f0daf.md
- Remaining: P3 doc sync (in progress), P4 scope ledger, P5 git
  hygiene, P6 housekeeping, P7 deferred refactor

## 2026-05-25 (commit c1fb551, branch main)

- Findings: Critical 0 | High 0 | Medium 3 | Low 2 | Info 3
- Routing: Mechanical 1 | Targeted-fix 0 | Multi-file 0 | Security-critical 0 | Config/CI 0 | Doc-edit 4 | Speculative-triage 0
- Report: audit-reports/audit-2026-05-25-c1fb551.md
- Top priority: Update `==WindhawkModReadme==` — user-facing Windhawk page description missing hover flyout, middle-click, crossfade, widget fade, smooth progress (not in working tree; requires new work)

## 2026-05-21 (commit c7a2675, branch main)

- Findings: Critical 0 | High 0 | Medium 1 | Low 3 | Info 4
- Routing: Mechanical 2 | Targeted-fix 1 | Multi-file 0 | Security-critical 0 | Config/CI 0 | Doc-edit 1 | Speculative-triage 0
- Report: audit-reports/audit-2026-05-21-c7a2675.md
- Top priority: Close stale OL-3 in OPEN_LOOPS.md — feature/rename-to-native-controller is fully merged into main (dc4c0d2) but the open loop still declares it unmerged
