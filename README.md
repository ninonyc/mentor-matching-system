# Mentor–Mentee Matching System

An AI-assisted class project that assigns mentees to mentors using ranked preferences and the Gale–Shapley Deferred Acceptance algorithm.

> **Status:** Deployed on https://ascent-mentoring.onrender.com/.

## Project goals

- Collect ranked preferences from mentors and mentees.
- Produce stable, one-to-one matches (maximum one mentee per mentor).
- Validate assignments and test the algorithm against edge cases.
- Provide a simple interface for mentors, mentees, and an administrator.

## Technology stack

| Component | Technology |
| --- | --- |
| Matching engine and API | C++17 |
| Frontend | GPT-generated UI using plain HTML, CSS, JavaScript, and SVG |
| Database | SQLite locally and on a Render persistent disk |
| Authentication | Password hashing with OpenSSL and bearer-token sessions |
| Testing | C++ core tests, exhaustive/randomized validation, Python database/API/integration tests, and Node.js frontend controller tests |
| Build tools | CMake for the core; shell build script for the API |
| Version control | Git and GitHub |
| Deployment | Docker on Render, with Caddy serving the frontend and proxying API requests |

## Implemented features

- Mentor, mentee, and administrator sign-in with server-enforced permissions.
- Ranked preference submission; changing a submitted ranking requires a written reason.
- Administrator preference overview and matching readiness checks.
- Mentor-proposing Gale–Shapley matching and an independent stability verifier.
- Saved assignments restored on sign-in, with explicit unassigned states.
- Participant feedback submission and a separate administrator feedback panel grouped by mentors and mentees.
- SQLite persistence for accounts, preferences, assignments, feedback, and matching history.
- A mountain-themed interface with dialog exit controls and responsive layouts.

Account creation, round creation, and roster management currently use administrator API endpoints; their management forms are not yet part of the UI. See [backend/API.md](backend/API.md).

## Folder overview

- `algorithm/include/` — public C++ headers for the matching engine and verifier.
- `algorithm/src/` — matching implementation, independent stability verifier, and demos.
- `tests/` — core validation and application integration tests.
- `database/` — database schema, migrations, and schema tests.
- `backend/` — C++ HTTP API, authentication, application logic, and API tests.
- `frontend/` — HTML, CSS, JavaScript, and visual assets.
- `deployment/` — container startup, reverse-proxy configuration, and SQLite backup helper.
- `tools/` — local integration demo and full-system test runner.
- `docs/` — initial design, roadmap, development logs, screenshots, and AI chat records.
- `Dockerfile` and `render.yaml` — container build and Render deployment configuration.

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

Implementation has progressed through all nine phases. Local automated suites and recorded manual acceptance checks passed. The hosted application is deployed with a working administrator login; testing the complete workflow online remains follow-up work.

## Build and run

Run commands from the repository root.

### Matching core

Requires a C++17 compiler and CMake 3.16 or newer. OpenSSL is not required for the core alone.

```bash
cmake -S . -B build/core -DCMAKE_BUILD_TYPE=Release
cmake --build build/core
ctest --test-dir build/core --output-on-failure
./build/core/matching_demo
```

### Local application

Requires C/C++ compilers, Python 3, and OpenSSL 3 development headers. On macOS with Homebrew, install OpenSSL with `brew install openssl@3`.

```bash
bash backend/build.sh
python3 tools/phase7_demo.py
```

Open [http://127.0.0.1:5173](http://127.0.0.1:5173). The launcher starts the API and frontend server and prints the credentials file location: `build/phase7-demo/accounts.json`. Use **Sign in** to exercise the backend. Sample-data exploration is illustrative and does not save changes or run the real matcher.

Keep the terminal open. Press **Ctrl+C** there to stop both servers. To reopen the existing test cohort without resetting its data:

```bash
python3 tools/phase7_demo.py --resume
```

If a port is already in use, stop the earlier server before restarting. Run other commands in a separate terminal while the application is running.

On the development machine running macOS 27.0, an OpenSSL deployment-target linker warning was resolved with:

```bash
MACOSX_DEPLOYMENT_TARGET=27.0 bash backend/build.sh
```

Use a deployment target compatible with your own OS and dependencies. See [backend/README.md](backend/README.md) for initializing a separate application database and administrator.

## Testing

```bash
python3 tools/run_full_system_tests.py
```

The runner builds the API and core tests, runs database/API/persistence suites, and saves timestamped logs and `results.json` under `build/phase8-results/`. Node.js is required for the frontend controller checks; without it, those checks are skipped and the report remains partial. Browser acceptance is tracked separately from automated results.

After building the API, deployment-specific checks can also be run:

```bash
node --test tests/integration/frontend_restore.cjs
python3 tests/integration/test_deployment.py
```

Validation covers matching correctness and stability, invalid inputs, unequal group sizes, exhaustive/randomized cases, database constraints, permissions, saved assignments and feedback, restart behavior, and frontend state restoration. Development logs record tests, errors, and corrections.

## Deployment

One Render Docker web service runs Caddy and the C++ API. Caddy serves the frontend and forwards `/api` requests to the backend. SQLite data is stored at `/var/data/mentoring.db` on a persistent disk to survive restarts and redeployments. This configuration requires paid Render compute and storage.

Automatic service deployments are disabled in `render.yaml`. Push changes to GitHub, then trigger a manual deployment in Render when ready.

The first successful startup creates an administrator from private Render environment settings, `MENTOR_BOOTSTRAP_EMAIL` and `MENTOR_BOOTSTRAP_PASSWORD`. These are application credentials. Changing those settings later does not reset the existing account. Never commit passwords, generated account manifests, or runtime databases.

The hosted database is separate from local test data; local test accounts are not uploaded automatically. Current limits include API-only account/round management, no self-service password recovery, and a single service instance. Full hosted participant and restart-persistence checks remain follow-up tasks.

## Development and AI assistance

This project uses AI-assisted development. The project author writes all prompts. Design decisions, tests, errors, checks, and changes are documented in `docs/`. Original prompts and AI conversations are preserved separately in `docs/ai-chat-logs/` where appropriate.

## Submission

The repository brings together the original preliminary design, code, tests, development logs, AI chat logs, technology list, screenshots, and deployed application link to document the implementation and development process.

## Agreed matching rules

- There are m mentors and n mentees, with m >= n.
- Mentors propose using Gale–Shapley deferred acceptance.
- Every participant ranks everyone on the opposite side exactly once, without ties.
- All pairs are acceptable; assignment is preferred to remaining unmatched.
- Every mentee receives one mentor; each mentor receives at most one mentee.
- Preferences stay fixed during each algorithm run.

Phases 1–3 establish the matching core. The database, backend, frontend, integration tests, and deployment build on that foundation.
