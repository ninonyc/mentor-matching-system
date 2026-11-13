## Phase 5 — C++ API and SQLite integration

**Date:** November 12, 2026

**AI implementation:** Added a C++17 local HTTP/JSON API with OpenSSL-backed
password hashing, expiring bearer sessions, account/round/roster management,
self-owned preference submission, readiness checks, real matching and
verification, historical run storage, rechecks against changed rankings,
participant-specific assignment views, partner ratings, and admin-only rating
reads. Request identity is derived from the token, not an owner/author field.

**Storage:** Numbered migration 002 adds credentials/sessions and run kind/
source. Version-1 assignments survive migration. Each matching/recheck run
is committed atomically with its exact submitted input versions. Rechecks
retain the original pairs and reference their source run. Earlier C++ matching
and stability implementations were not changed.

**Dependencies:** Bundled cpp-httplib v0.66.0 (MIT), nlohmann/json v3.12.0
(MIT), and SQLite amalgamation 3.50.4 (public domain). Source URLs, upstream
licenses, and SHA-256 checksums included. OpenSSL is a system dependency.

**Build:** Direct bash build and CMake API-enabled Release build succeeded
in Linux x86_64 with GCC 14.2.0 and OpenSSL 3.5.7. SQLite compilation uses
-O2 to match the direct route. The API CMake option defaults OFF so earlier
core-only builds do not require OpenSSL.

**Checks actually executed here:**
- Final direct-build executable: all 33 HTTP integration tests passed
  (9.107 seconds in the recorded run).
- CMake/CTest: all five suites passed (9.19 seconds total in that run).
- CTest covered Phase 1's 12 scenarios, Phase 2's 25 scenarios and witness
  check, all Phase 3 validation, 33 API tests, and 31 schema tests.
- API tests launch the real C++ server against isolated temporary databases.
- Tests cover malformed/type/oversized/deep JSON, origin handling, session
  expiry/logout, password changes and revocation, deactivation, role/ownership
  checks, rollback, actual matching, input-version snapshots, stable and
  unstable rechecks, rating privacy, lifecycle constraints, migration of
  version-1 history, SQL injection-shaped input, rate limiting, and bootstrap
  lockout after initial setup.

**Evidence:** `phase5-api-tests-linux.txt` contains the final direct-build
HTTP test output. Earlier CMake runs exercised all five suites successfully.
No API sanitizer run is claimed in Phase 5. Earlier core sanitizer evidence
remains separate. The author subsequently confirmed the local Mac result below.

**Implementation defaults:** Admin-created accounts, eight-hour sessions,
20 login attempts/source-IP/minute, 64 KiB/32-level JSON limits, one exact
optional browser origin, maximum 2,000 participants/round, one serialized
SQLite connection, loopback-only HTTP. These can be reviewed during integration.
No public server, HTTPS hosting, email/password recovery, or UI was deployed.

**Status:** API implementation/build/environment tests complete; author-local build and HTTP tests confirmed below; code review pending. No Git commit/push.
Existing development logs/screenshots are excluded from this update.

## Local test P5-M01 — HTTP integration suite

**Date:** November 12, 2026

**Tester/environment:** Project author, local Mac / VS Code workflow.

**Procedure:** Run `python3 backend/tests/test_api.py` after building the API.

**Expected:** All 33 tests pass; final unittest summary ends with `OK`.

**Reported:** Author stated that 33 tests ran in 4.736 seconds and that
files were uploaded. In a follow-up, the author explicitly confirmed that
the final unittest summary ended with `OK`.

**Result:** PASS — all 33 HTTP integration tests passed locally.
The elapsed time is recorded as an observed duration, not a performance target.

**Scope:** This records the author-local run separately from the previously
verified Linux implementation-environment results. No manual application
startup, frontend integration, deployment, or GitHub push is inferred.
