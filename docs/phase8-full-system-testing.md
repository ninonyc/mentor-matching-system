# Phase 8 — Full-system testing

The Phase 7 local workflow works. Phase 8 checks the complete project together
and records failures and untested behavior before deployment. The production
backend remains C++; Python drives isolated test processes.

## Install

Copy this update's folders into your existing project root, preserving paths.
It adds documentation, a test runner and existing integration scripts, and
replaces two HTTP test/client helpers to close expected HTTP error responses.
It includes no database, credentials, screenshots or new UI design changes.
Your Phase 1–6 development logs and main development-log.md are not overwritten.

## Run from the VS Code terminal

Stop any running Phase 7 launcher with Ctrl+C and wait for the `%` prompt.
From your project root, run:

```bash
python3 tools/run_full_system_tests.py
```

The runner builds the C++ API, compiles/runs three core suites, runs the 31
schema tests, 34 HTTP tests and three persistence/port checks, then runs the
six frontend controller checks if Node is available. It records each exit
status, duration, command, log path, environment and source hashes in a unique
folder beneath `build/phase8-results/`.

On Mac it uses the current macOS version as the default deployment target;
an explicitly set MACOSX_DEPLOYMENT_TARGET takes precedence. You can still
supply it explicitly:

```bash
MACOSX_DEPLOYMENT_TARGET=27.0 python3 tools/run_full_system_tests.py
```

If the backend is already rebuilt from the current source:

```bash
python3 tools/run_full_system_tests.py --skip-build
```

That report explicitly marks the backend build NOT_RUN. Core executables are
still compiled so the three core suites use current source.

Expected ending when all invoked checks pass and Node is available:

```text
EXECUTED_CHECKS_PASSED
Browser acceptance: pending
Report: .../build/phase8-results/.../results.json
```

A missing Node installation is recorded as SKIP and the report says PARTIAL;
it is not silently counted as a pass. The existing manual desktop UI remains
usable without Node. A failure returns a nonzero exit status and names the
per-check text log. Tests use temporary HTTP databases and do not modify your
`build/phase7-demo/mentoring.db`. Do not upload accounts.json as evidence.

The aggregate command does not include a browser by default. If Node,
Playwright and Chromium are already set up as described in the Phase 7 guide,
and ports 8080 and 5173 are free, opt in with:

```bash
python3 tools/run_full_system_tests.py --browser
```

The optional browser workflow is not previously validated by a successful run
in the authoring environment. A browser setup/launch failure is reported as a
failure or blocker, not as application success. Existing Playwright setup
instructions remain in `docs/phase7-integration.md` from the earlier update.

## Manual acceptance matrix

Run the live cohort with `python3 tools/phase7_demo.py --resume`. Keep the
launcher running while using the browser. Do not select sample-data mode.
Use the same existing cohort, so previous assignments and feedback can be checked.
Tests that create new matchings change only this local test cohort and preserve
historical runs. Read each test's scope before changing rankings.

| ID | Action | Expected | Current record |
| --- | --- | --- | --- |
| P8-M01 | Restart launcher, hard-refresh browser, log in as admin | Existing pairs and Feedback From: panel load without pressing Assign | Targeted repeat pending; UI fix accepted in Phase 7 |
| P8-M02 | Sign in as an assigned mentor and an assigned mentee after restart | Correct saved partner and saved ranking order appear | Full role-by-role post-fix repeat pending |
| P8-M03 | Sign in as Sam | Verified unassigned state, no partner-rating form | Passed in Phase 7; repeat with final bundle pending |
| P8-M04 | Read feedback as admin; sign out and open a participant workspace | Admin sees correct author/partner/score/comment; participant has no administrator feedback panel | New panel accepted; exact cross-role repeat pending |
| P8-M05 | Open each login entrance; dismiss with ×, Back, Escape and outside click | Dialog closes; no login or assignment request is created | Complete keyboard/cancel matrix pending |
| P8-M06 | Open matching confirmation; cancel via ×, Back and Escape | Saved result is unchanged; no extra matching run | Browser confirmation sequence pending; API matching and controller checks separate |
| P8-M07 | Wrong password, then correct password | Wrong credentials rejected; successful sign-in restores the correct role/round | API negative cases passed; browser action pending |
| P8-M08 | Change a saved ranking; submit without reason, then with reason | Missing reason rejected; explained change saved; historical pairs marked outdated, not automatically unstable | API coverage passed; browser sequence pending |
| P8-M09 | Temporarily stop backend while leaving frontend visible, then sign in/load data; restart with --resume | Clear connection/load error; no fabricated empty assignment; recovery succeeds | Browser recovery sequence pending |
| P8-M10 | With a separate second-round fixture, switch rounds | Rankings, assignments and feedback belong only to selected round | Controller filtering passed; two-round live browser setup pending |
| P8-M11 | Refresh browser while signed in | Returns to login because tokens are held in memory; subsequent login restores saved data | Targeted repeat pending |
| P8-M12 | Reopen feedback after another server restart | Existing submitted feedback remains; do not submit duplicates solely to test persistence | HTTP persistence passed; explicit browser repeat pending |

For each performed case record PASS/FAIL, actual observation, environment and
evidence filename. Use NOT_RUN/BLOCKED when appropriate. Do not turn expected
behavior into a passed observation. If an unexpected matching or missing result
appears, record the selected round and account role before rematching.

## Acceptance and release gate

Phase 8 can close when the author's consolidated run has been reviewed,
applicable desktop manual cases have recorded outcomes, and unresolved defects
have explicit decisions. Optional automated browser checks may remain blocked
if equivalent manual coverage is recorded. This is a desktop class-project
release; mobile polish and API-only management screens should be documented
as scope boundaries, rather than silently claimed complete.

Deployment is Phase 9 and has not started. No server is made public by the
Phase 8 runner. Deployment planning must address HTTPS, hosted persistent
SQLite data, credentials, backups, and a repeatable startup before publishing.

## Review questions

1. What does the aggregate runner prove, and which browser interactions does it not prove?
2. Why distinguish a missing assignment from a failed assignment request?
3. Why must test credentials and databases stay outside committed evidence?
4. Why does changing preferences make an earlier run outdated without proving instability?
5. How do persisted run IDs and source hashes help investigate a regression?
