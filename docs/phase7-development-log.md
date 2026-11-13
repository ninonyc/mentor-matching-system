# Phase 7 — Integration development log

Implemented a standard-library Python launcher for an isolated live cohort:
three mentors, two mentees, administrator, frozen roster, initially missing
rankings. Starts the real C++ API and static frontend with exact allowed
origin. Writes generated fixture credentials under the ignored build tree;
refuses existing directories and busy ports. Explicit resume preserves SQLite
records. No existing application database is reused.

Added one HTTP workflow test covering preference submission, stable matching,
participant-specific pairs, feedback, forbidden rating reads, static JavaScript
serving, backend restart, persisted records, integrity, and overwrite refusal.
Added optional Playwright browser workflow and an ID-based manual checklist.

| ID | Check | Result |
| --- | --- | --- |
| P7-A01 | Existing API regression suite | PASS: 34 tests in 8.984 seconds, Linux. |
| P7-A02 | New HTTP persistence workflow | PASS: one workflow test in 4.548 seconds, Linux. |
| P7-A03 | Browser runner JavaScript syntax | PASS. Does not validate browser behavior. |
| P7-B01 | Automated browser execution | BLOCKED: Chromium launch reports socket operation not permitted. |
| P7-M01–M08 | User-local browser checklist | PENDING. |

The native browser fallback also could not enter the integration URL because
its accessibility provider was unavailable. No live browser outcome is claimed.
Mac execution, user screenshots, and Phase 7 acceptance remain pending.
Nothing has been pushed or deployed. The initial integration harness required no production changes. The restart
correction below updates the backend socket options.

User-local evidence: API suite PASS, 34 tests in 4.791 seconds, with OK
confirmed by screenshot. Initial workflow PASS, one test in 2.893 seconds.
Launcher resume reported Errno 48 Address already in use. Reproduced a false
busy-port result with recently closed sockets, then found that the HTTP
library default uses SO_REUSEPORT. Updated the probe and backend to
SO_REUSEADDR; active listeners remain excluded. Added two port regression
tests and strengthened the persistence test to resume on the same ports.
The separate macOS linker warning reports a 26.0 deployment target with an
OpenSSL library built for 27.0. Actual OS version is not yet confirmed.

Restart correction validation: PASS, three integration tests in 4.542 seconds;
API regression PASS, 34 tests in 9.288 seconds, Linux. Mac restart verification
remains pending. No macOS deployment-target change has been made.
