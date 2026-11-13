## Phase 4 — SQLite database design

**Date:** November 12, 2026

**Database decision:** Author selected SQLite.

**AI implementation:** Migration 001 defines ten tables and a freshness view
for users, round-specific roles, versioned rankings/change reasons, immutable
run input references, assignments, verification evidence, and partner ratings.
Added an in-memory/persistent demo fixture and schema tests using Python's
standard-library sqlite3 module. Backend language remains C++; database-to-core
integration, authentication, API endpoints, and UI are not implemented here.

**Defaults introduced for review:** Freeze roster before preference submission;
subsequent roster changes/promotions use a new round; ratings are append-only
and use an integer 1–5 scale. User deactivation preserves existing frozen rounds.

**Environment:** Linux; Python sqlite3 module reporting SQLite 3.53.1.

**Validation:** Database demo passed; 31 schema tests passed. Tests cover
foreign keys, case-insensitive email uniqueness, frozen rosters, promotions
preserving history, complete/unique ranks, change explanations, immutable
submitted preferences, exact run input ownership, administrator requester
records, assignment roles/uniqueness/completeness, verifier status agreement,
blocking-pair shape, finalized-history protection, partner-only rating
relationships, changed-input freshness, round closure, and atomic rollback.
Includes checks that INSERT OR REPLACE cannot bypass protected history when
required connection options are enabled. See `phase4-schema-tests-linux.txt`.

**Persistence check:** Created and reopened a temporary on-disk demo database;
SQLite integrity_check returned ok. A repeat demo invocation refused to overwrite
the existing file. No persistent demo data is included in the delivery archive.

**Demo limitation:** Known fixture verification is seeded by inspection; the
fixture does not invoke the C++ matcher/verifier. It demonstrates storage only.

**Application responsibilities:** Trusted C++ code must compute stability;
HTTP authentication/authorization must enforce admin actions, preference
ownership, and admin-only rating reads. SQLite schema does not enforce reader
identity. No password or login-session feature was added.

**Status:** Schema and initial checks complete in implementation environment;
author-local demo and schema tests confirmed below; lifecycle/design review remains pending. No existing logs,
screenshots, production C++ code, CMake files, or GitHub repository modified.

## Manual test P4-M01 — Local SQLite demo

**Date:** November 12, 2026

**Tester/environment:** Project author, local Mac / VS Code terminal.

**Command:** `python3 database/demo.py`

**Observed output:**

```text
SQLite schema version: 1
Users: 6
Participants: 5
Run state: verified
Assignments: 2
Inputs current: True
Foreign-key check: OK
DATABASE DEMO PASSED
```

**Result:** PASS, shown in the supplied screenshot. This is the in-memory
fixture demonstration, not a C++ matcher/database integration test.

## Manual test P4-M02 — Local schema test suite

**Date:** November 12, 2026

**Tester/environment:** Project author, local Mac / VS Code terminal.

**Command:** `python3 database/tests/test_schema.py`

**Expected:** All 31 tests pass; unittest ends with `OK`. Elapsed time varies.

**Observed:** Author reported `Ran 31 tests in 0.043s`. Screenshot shows
each of the 31 named test cases marked `ok`; the final unittest summary
is outside the image.

**Result:** PASS, supported by the visible individual results and author report.

**Evidence:** Uploaded `database test.png`; author can retain it in
`docs/screenshots/`. Screenshot is not copied into this update archive.

**Scope:** Local schema integrity and fixture checks passed. Persistent-file
creation/reopening, authentication, HTTP authorization, and C++/SQLite
integration have not been reported as tested locally. No production code changed.
