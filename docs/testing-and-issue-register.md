# Test and issue register — Phases 1–7

Date: November 12, 2026 (project author's America/New_York time).

This register consolidates the project's reported execution evidence. A user
confirmation records only the behavior confirmed. Linux checks, Mac checks,
manual observations and unexecuted browser scripts are distinguished. Test
counts and elapsed times are observations, not guarantees or coverage proofs.
Earlier phase logs remain historical; later phases supersede their earlier
statements that database, API or UI work was still pending.

## Confirmed test results

| ID / phase | Check | Implementation environment | Author's Mac / live UI |
| --- | --- | --- | --- |
| P1-M01 | Matching demo, four mentors / three mentees | Expected assignments and nine proposals; 12 smoke scenarios passed | Expected demo output confirmed |
| P1-I01 | C++17 IntelliSense configuration | Source requires C++17 for optional | Red underlines cleared after selecting c++17; before/after screenshots recorded by author |
| P2-M01 | Independent verifier demo | Stable assignment accepted; blocking pair reported for unstable example | Expected output confirmed |
| P2-M02 | Verifier suite | 25 scenarios plus blocking-pair ID check passed independently of matcher linkage | Same expected output confirmed |
| P3-M01 | Exhaustive / randomized / mutation / contention correctness | 46,973 profiles; 281,723 candidate assignments; 1,000 seeded cases; 866 × four mutation checks; eight contention cases passed | Expected correctness output confirmed |
| P3-M02 | Benchmarks | 12 instances measured, three repetitions each | Completion confirmed; screenshot contains 11 visible result rows |
| P4-M01 | SQLite schema demo | Fixture persistence and integrity checked | DATABASE DEMO PASSED shown |
| P4-M02 | Schema suite | 31 checks passed | 31 tests in 0.043 seconds; visible test cases marked ok |
| P5-M01 | Initial C++ API integration | 33 HTTP tests and five CMake suites passed | 33 tests in 4.736 seconds, final OK explicitly confirmed |
| P6-A01 | Administrator preference overview addition | API suite expanded to 34 tests and passed | Later confirmed by P7-M09 below |
| P7-M09 | Corrected API regression suite | 34 tests passed in latest Phase 7 run, 9.288 seconds | 34 tests in 4.791 seconds, OK visible |
| P7-M10 | Initial persistent workflow | One HTTP workflow passed | One test in 2.893 seconds, OK visible |
| P7-M11 | Restart regression suite | Three tests passed in 4.542 seconds; later 4.606 seconds after UI changes | Three tests in 3.174 seconds, OK visible |
| P7-A04 | Frontend restoration / feedback controller | Six dependency-free Node checks passed | Separate execution of this Node suite not reported |
| P7-M12 | Live administrator login and frozen cohort | API fixture has three mentors and two mentees | Author confirmed live login and those counts |
| P7-M13 | Real participant preference submissions | API workflow checks all lists and readiness | Author followed five-account submission instructions and reported success |
| P7-M14 | Real matching and unassigned mentor | Matching and filtered assignment APIs covered | Author confirmed Sam is currently unassigned after matching |
| P7-M15 | Live feedback submission | API ownership and administrator-only reads covered | Author explicitly confirmed submitting feedback; exact number/values not recorded |
| P7-M16 | Restart and saved rankings | HTTP suite verifies database records after same-port restart | Author confirmed saved preference order after restarting |
| P7-M17 | Saved-result display and Feedback From panel | Six controller checks and three persistence tests passed | Author reported “great it works!” after installing correction |

The final confirmation accepts the corrected UI in the user's live setup.
A second complete post-fix restart with every role has not been explicitly
reported; Phase 8 includes that as a manual acceptance check. Exact Alex/Jordan
and Morgan/Casey pairs were expected for unchanged default rankings, but the
conversation explicitly confirms Sam's unassigned state rather than a complete
transcript of both other pairs. No additional observation is invented.

## Errors, warnings and corrections

| Issue | Symptom / finding | Correction | Evidence and current status |
| --- | --- | --- | --- |
| I01 — Editor standard | std::optional and related fields underlined despite a working terminal build | Set VS Code C/C++ IntelliSense to c++17 | RESOLVED: author confirmed underlines disappeared; no algorithm change |
| I02 — Commands typed into a running server | Build/test commands appeared below server output without executing | Stop the foreground server with Ctrl+C, wait for the % shell prompt, or use another terminal | RESOLVED in workflow; later commands executed and tests passed |
| I03 — Missing Phase 6 backend update | API suite ran 33 tests rather than expected 34 | Replace backend/src/service.cpp and backend/tests/test_api.py from Phase 6, then rebuild | RESOLVED: 34 tests / OK visible |
| I04 — Restart port probe | Errno 48 Address already in use could occur after stopped connections | Enable SO_REUSEADDR in launcher probe and explicitly in backend; avoid the HTTP library's default SO_REUSEPORT behavior | FIX VALIDATED: recently closed port, active-listener rejection and same-port restart tests passed on Linux and author's Mac |
| I05 — A genuinely busy port | Updated launcher identified port 8080 as unavailable | Stop the old foreground server in its own terminal; lsof command identifies any remaining listener | RESOLVED locally: later screenshot shows Live integration ready after --resume |
| I06 — macOS linker warning | Build targeted 26.0; Homebrew OpenSSL libcrypto dylib was built for 27.0 | Confirmed actual macOS is 27.0; use MACOSX_DEPLOYMENT_TARGET=27.0 bash backend/build.sh | RESOLVED for reported build: subsequent screenshot shows command completed without that warning; no OS downgrade or library replacement |
| I07 — Expected HTTP errors left open in tests | Python 3.15 ResourceWarning during negative 403/413/429 cases; suites still ended OK | Phase 8 closes HTTPError response objects explicitly in the API test client and integration launcher | IMPLEMENTED; Linux verification recorded in Phase 8 results; user's Python 3.15 rerun pending. Do not suppress the warnings |
| I08 — Saved assignment appeared missing | After restart rankings appeared but no saved result was visible; administrator UI never fetched prior runs on login | Load saved runs on login, round change and Refresh. Participant missing-run, unmatched and load-error states now differ, with Retry for errors | RESOLVED UI: author accepted update. Specific user database was not inspected; no data loss or recovery is claimed |
| I09 — Administrator feedback not displayed | Participant could submit feedback, but administrator UI had no reading panel | Add separate Feedback From: panel below workspace, matched width/minimum height, Mentor/Mentee columns, author/partner, score/comment/run | IMPLEMENTED AND USER ACCEPTED; scoped to selected-round assignment IDs and administrator-only API reads |
| I10 — Exit / layout refinements | Need explicit exits, hover lift, clearer instruction alignment, no summit admin button | Added Back/× controls and dismissible matching confirmation; kept footer admin link; aligned darker instruction below Rise together | User accepted revised interface; exhaustive desktop keyboard/cancel coverage remains Phase 8 checklist |
| I11 — Browser automation environment | Chromium launch denied a socket; native accessibility unavailable; cloud browser could not reach workspace loopback | Used isolated HTTP persistence tests plus controller checks; supplied optional browser workflow | STILL AN ENVIRONMENT LIMITATION: automated browser suite is not recorded as passed |

The administrator display correction used existing APIs and did not delete,
recreate or rematch the author's data. The full matching theorem continues to
assume complete strict preference rankings, m >= n, and one-to-one assignment.

## Screenshot evidence index

These are source upload names. The author may retain/rename copies under
`docs/screenshots/`; this package does not copy or overwrite screenshot files.

| Source name | Evidence |
| --- | --- |
| hpp before.png / hpp after.png | IntelliSense issue and correction; author reported both added |
| benchmark.png | Local benchmark evidence; original upload was Screenshot 2026-11-12 at 19.35.50.png |
| database test.png | Database demo and schema results |
| Screenshot 2026-11-12 at 21.21.31.png | 33-test stale suite, one workflow pass, original address-in-use error |
| Screenshot 2026-11-12 at 21.26.00.png | Stopping static server, successful launcher, shell commands typed into foreground launcher |
| Screenshot 2026-11-12 at 21.27.52.png | 34 tests / 4.791s / OK; original resume port error |
| Screenshot 2026-11-12 at 21.29.09.png | Linker version warning and Ctrl+C traceback during interrupted testing |
| Screenshot 2026-11-12 at 21.35.24.png | Actual macOS 27.0, corrected build command, three tests / 3.174s / OK, explicit 8080 conflict |
| Screenshot 2026-11-12 at 21.37.18.png | Successful resume, live URL, generated accounts file path |

A Ctrl+C traceback documents interruption, not a completed failed test suite.
Credentials/tokens are excluded from this register and delivery. No repository
commit, push, public hosting, HTTPS or deployment is inferred from these tests.

## Carry-forward checks

Phase 8 is full-system acceptance, including role-by-role restart behavior,
feedback visibility, changes to rankings, cancellation, empty states, wrong
credentials, cross-round isolation, and recorded negative cases. Existing API
checks cover many of these programmatically; browser actions require distinct
evidence. Deployment is Phase 9 and has not started.


## Phase 8 author-local follow-up

Screenshot 2026-11-12 at 22.21.23.png confirms a successful API build and
passing smoke, verifier, core-validation, schema, API and persistence checks.
Final result is PARTIAL only because Node is absent and frontend-controller
checks were skipped. Browser acceptance remains pending. See the Phase 8 log
for individual durations. No failures were reported among executed checks.
The earlier static-server favicon.ico 404 is a missing browser icon, not a
matching/API test failure; it is a cosmetic item rather than lost data.


Phase 8 follow-up P8-M01: author confirmed all six frontend controller tests
passed. All automated suites now have local pass evidence. The initial
PARTIAL aggregate report remains accurate for that invocation. Manual
browser acceptance is still pending; no browser automation pass is claimed.
