# Phase 7 closeout — integration

Date: November 12, 2026, America/New_York.

Integration is accepted for the local class-project workflow. The author
successfully used real login, submitted participant rankings, triggered matching,
confirmed Sam is unassigned, submitted feedback, and accepted the restored
assignment display plus administrator Feedback From: panel. This acceptance
covers the exercised workflow; broader negative/manual checks move to Phase 8.
No public deployment is complete.

## Recorded author-local results

| Check | Observation | Result |
| --- | --- | --- |
| Initial HTTP workflow | One test, 2.893 seconds, OK | PASS |
| Corrected API suite | 34 tests, 4.791 seconds, OK | PASS |
| Restart/port workflow suite | Three tests, 3.174 seconds, OK | PASS |
| Launcher resume | Live integration ready at 127.0.0.1:5173 | PASS after stopping the old server |
| macOS build | Actual OS 27.0; matching deployment-target command completed without the prior 26.0 warning | PASS for reported build |
| Administrator login | Three mentors and two mentees displayed | PASS |
| Participant preferences | Submission flow worked; saved order persisted across restart | PASS, author-confirmed |
| Matching and assignments | Matching worked; Sam explicitly confirmed unassigned | PASS for reported behavior |
| Feedback submission | Author confirmed feedback submitted | PASS; exact number and scores not recorded |
| Administrator saved-result/feedback correction | Author said “great it works!” after update | ACCEPTED for reported UI behavior |

Do not reinterpret the last row as an additional full role-by-role post-fix
restart test; that remains on the Phase 8 manual checklist. No direct inspection
of the author's SQLite file was performed. The original missing-result report
was traced to an administrator loading defect, not established data loss.

## Corrections retained

The launcher and backend permit immediate restart with SO_REUSEADDR while
refusing active listeners. Early probe-only correction was insufficient: the
same-port restart regression exposed the backend HTTP library's SO_REUSEPORT
default, leading to the explicit backend override. The final three-test suite
passed both in Linux and on the author's Mac.

Live setup must run at a shell prompt. Commands typed into a running static
server or launcher do not execute as shell commands. Ctrl+C stops that foreground
process; use --resume to reopen the existing cohort rather than recreate it.

The macOS 26.0/27.0 message was a linker compatibility warning. The author
reported macOS 27.0; setting MACOSX_DEPLOYMENT_TARGET=27.0 for the build removed
the reported warning. The Phase 8 runner defaults to the current Mac version
when no explicit deployment target is supplied.

Saved assignments now reload on administrator login, selected-round changes,
and Refresh. Participant views distinguish no published assignment, verified
unassigned, and load failure. The separate Feedback From: panel groups submissions
by mentor/mentee author and filters them to selected-round assignments.

The Python ResourceWarning did not change passing test outcomes. Phase 8 adds
explicit HTTPError response cleanup rather than suppressing the warning; local
Python 3.15 verification remains pending.

## Remaining boundaries

The optional Playwright browser runner has not passed in the authoring environment.
Browser launch/access was blocked; six controller checks and real HTTP persistence
tests are recorded separately. Existing core sanitizer evidence excludes leak
detection. No API sanitizer result is claimed. Full history/recheck/account/roster
management UI screens remain absent; existing API routes provide those operations.

`testing-and-issue-register.md` is the cumulative evidence and correction index.
Keep earlier development logs as historical records and retain existing screenshots.
Phase 8 is full-system acceptance; Phase 9 is deployment.
