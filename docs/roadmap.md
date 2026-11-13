# Development roadmap

Status updated November 12, 2026 from implemented work and author-confirmed local runs.

- [x] Phase 1 — C++ Gale–Shapley matching core; demo verified locally.
- [x] Phase 2 — Independent stability verifier; demo and focused suite verified locally.
- [x] Phase 3 — Exhaustive small cases, random/mutation/contention tests and benchmarks; local execution confirmed.
- [x] Phase 4 — SQLite schema; local demo and 31 schema checks passed.
- [x] Phase 5 — C++ API, authentication, authorization and SQLite/core connection; local HTTP suite passed.
- [x] Phase 6 — Desktop mountain interface; exits, ranking workspaces and admin preference overview implemented and used locally.
- [x] Phase 7 — Live UI/API/database workflow accepted; restart corrections and saved-result/feedback display added.
- [ ] Phase 8 — Full-system acceptance: automated runner delivered and executed in Linux; author-local consolidated run and remaining manual cases pending.
- [ ] Phase 9 — Deployment: hosting, HTTPS, persistent runtime storage and operational verification pending.

A checked implementation milestone does not assert every possible test passed.
See `testing-and-issue-register.md` for precise evidence and limits, and
`phase7-closeout.md` for integration acceptance. The optional automated browser
suite remains unverified; manual acceptance and regression checks continue in Phase 8.
No Git push, production deployment or public exposure is recorded here.
