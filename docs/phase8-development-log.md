# Phase 8 — Full-system testing implementation

Date: November 12, 2026 (America/New_York; run timestamps use UTC).

Added a single runner for existing matching, verifier, exhaustive/randomized,
schema, HTTP API, persistence/port and frontend-controller checks. Each run
gets a unique ignored build directory with logs and a JSON report containing
commands, statuses, timings, environment and source SHA-256 fingerprints.
Node absence is explicit SKIP/PARTIAL; browser checks are opt-in and separately
reported. No existing application database is used.

Corrected HTTPError response cleanup in backend/tests/test_api.py and
tools/phase7_demo.py. The expected negative HTTP status is still propagated
and tested; response resources are now closed instead of left to garbage
collection. Warnings are not suppressed. Author's Python 3.15 check is pending.

Consolidated earlier tests and issues in testing-and-issue-register.md; added
phase7-closeout.md and updated roadmap. Original phase logs/screenshots are
retained, and later-phase status is distinguished from earlier historical
“pending” statements. Author prompts were not generated or rewritten.

## Executed validation

Ran python3 tools/run_full_system_tests.py --skip-build in Linux using the
already-built, previously verified C++ API. Backend build was explicitly
NOT_RUN for this aggregate invocation; no new backend production change was
made. The three core test binaries were freshly compiled.

| Check | Result |
| --- | --- |
| P8-build-api | NOT_RUN |
| P8-build-matching_smoke_tests | PASS · 1.37 seconds |
| P8-matching_smoke_tests | PASS · 0.004 seconds |
| P8-build-stability_tests | PASS · 1.322 seconds |
| P8-stability_tests | PASS · 0.008 seconds |
| P8-build-core_validation | PASS · 2.428 seconds |
| P8-core_validation | PASS · 0.368 seconds |
| P8-schema | PASS · 0.165 seconds |
| P8-api | PASS · 9.352 seconds |
| P8-persistence | PASS · 4.581 seconds |
| P8-frontend-controller | PASS · 0.215 seconds |

All invoked executable suites passed. API suite: 34 tests; schema suite: 31;
persistence/port suite: three; controller suite: six. Core output retains its
own scenario/exhaustive counts in the evidence logs; these are not summed
into an invented single test count. No ResourceWarning is present in the
recorded Linux API/persistence logs. Python 3.15 behavior remains unverified.

Evidence: phase8-linux-evidence/results.json and per-check text logs.

## Pending

Author-local frontend controller checks, final role-by-role manual acceptance
and optional browser execution. Browser limits from Phase 7 remain recorded separately.
No test result is claimed for the author's Mac until reported. No new
sanitizer, peak-memory, production load or deployment result is inferred.
Nothing was committed, pushed, or deployed.


## Author-local aggregate run P8-M00 — PARTIAL, no executed failures

Evidence: Screenshot 2026-11-12 at 22.21.23.png. Author's Mac, macOS 27.0,
Python 3.15, VS Code shell terminal. Ran the default command (backend build
included). The screenshot's final result is PARTIAL, browser acceptance pending.

| Check | Observed result | Seconds |
| --- | --- | ---: |
| API build | PASS | 12.295 |
| Matching smoke build | PASS | 0.674 |
| Matching smoke execution | PASS | 0.134 |
| Verifier build | PASS | 0.585 |
| Verifier execution | PASS | 0.076 |
| Core validation build | PASS | 1.055 |
| Core validation execution | PASS | 0.300 |
| Schema suite | PASS | 0.094 |
| API suite | PASS | 4.955 |
| Persistence/port suite | PASS | 3.021 |
| Frontend controller | SKIP: Node is not installed | — |

All executed checks passed. This is not full Phase 8 acceptance: the six
controller checks did not run locally and the manual browser matrix remains.
The screenshot shows ResourceWarning reporting enabled, but per-suite logs
were not supplied, so warning-free Python 3.15 execution is not asserted.
The previous Linux evidence remains separate from this Mac observation.

Next: install Node through the existing Homebrew setup, then run only the
skipped suite: node --test tests/integration/frontend_restore.cjs. Repeating
already-passed backend/core suites is unnecessary absent further code changes.
Optional browser automation remains distinct from these controller tests.


## Author-local follow-up P8-M01 — frontend controller PASS

The author explicitly reported “all 6 tests passed” after the skipped Node
controller suite was addressed. Result: PASS, six checks. Duration, Node
version and a new screenshot were not supplied and are not inferred.

All automated suites have now passed locally across the aggregate run and
this separate controller run. The earlier aggregate report remains PARTIAL
as an immutable record of what that invocation actually ran; it is not
rewritten to invent a consolidated rerun. Browser acceptance remains pending.
No code changed and no repetition of passing suites is required.
