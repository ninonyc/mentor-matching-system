# Mentor–Mentee Matching System

An AI-assisted class project that assigns mentees to mentors using ranked preferences and the Gale–Shapley Deferred Acceptance algorithm.

> **Status:** Deployed on https://ascent-mentoring.onrender.com/.

## Project goals

- Collect ranked preferences from mentors and mentees.
- Produce stable, one-to-one matches (maximum one mentee per mentor).
- Validate assignments and test the algorithm against edge cases.
- Provide a simple interface for mentors, mentees, and an administrator.

## Planned stack

| Component | Planned technology |
| --- | --- |
| Matching engine and API | C++ |
| Frontend | Undecided; evaluate a GPT-generated UI first, then Lovable |
| Database | To be finalized (SQLite locally or hosted PostgreSQL) |
| Testing | C++ tests and randomized test generation |
| Version control | Git and GitHub |
| Deployment | Netlify frontend, Render backend (to be verified when deploying) |

## Folder overview

- `algorithm/include/` — public C++ headers for the matching engine.
- `algorithm/src/` — matching implementation and, in Phase 2, an independent stability verifier.
- `tests/` — tests of the matcher and verifier, edge cases, and randomized test generation.
- `database/` — database schema and migrations.
- `backend/` — C++ HTTP API and application logic.
- `frontend/` — UI code once exported from Lovable.
- `docs/` — initial design, roadmap, development logs, screenshots, and AI chat records.

## Nine-phase development roadmap

1. Implement Gale–Shapley matching algorithm in C++.
2. Build an independent stability verifier.
3. Validate correctness, edge cases, randomized cases, and algorithmic performance.
4. Design the database schema.
5. Implement the C++ backend API.
6. Build a minimalistic frontend.
7. Integrate the frontend, backend, and database.
8. Test the complete application.
9. Deploy the live application.

## Build and run

The implementation has not started. `CMakeLists.txt` defines the C++ project settings, but there are no executable targets yet. Build and test instructions will be added with Phase 1.

## Development and AI assistance

This project uses AI-assisted development. The project author writes all prompts. Design decisions, tests, errors, checks, and changes will be documented in `docs/`. The original prompts and AI conversations will be preserved separately in `docs/ai-chat-logs/` where appropriate.

## Submission

The final repository will include the original preliminary design, code, tests, development log, AI chat logs, technology list, screenshots, and (if available) a link to the deployed application.

## Agreed matching rules

- There are m mentors and n mentees, with m >= n.
- Mentors propose using Gale–Shapley deferred acceptance.
- Every participant ranks everyone on the opposite side exactly once, without ties.
- All pairs are acceptable; assignment is preferred to remaining unmatched.
- Every mentee receives one mentor; each mentor receives at most one mentee.
- Preferences stay fixed during each algorithm run.

Phases 1–3 form the core matching milestone. Later subsystems remain placeholders until that milestone is established.
