# Phase 5 — C++ HTTP API

## Install the update

Copy the archive contents into the existing project root, keeping paths.
Replace `CMakeLists.txt`; the backend files, migration 002, and Phase 5 docs
are new. Do not replace the repository folder or its `.git` directory.
Existing logs/screenshots and production matching/verification code are excluded.

The backend is C++17. Python is used only to drive automated HTTP tests, as
in Phase 4's database utilities. The server uses SQLite's C API directly.

## Build and run the integration tests first

Requirements: a C/C++17 toolchain, Python 3.8+, OpenSSL 3 headers/library.
HTTP/JSON/SQLite sources are bundled; there is no CMake network fetch.
On a Mac using Homebrew, install the OpenSSL dependency if needed:

```bash
brew install openssl@3
```

From the project root:

```bash
bash backend/build.sh
python3 backend/tests/test_api.py
```

Expected final summary: `Ran 33 tests ...` followed by `OK`. Timing varies.
Tests start their own C++ server on a free local port, create a temporary
database and fictional accounts, then stop the server and remove the database.
You do not need to start a server manually for these tests. They never target
your application database. The sample test password is fictional and disposable.

The build script locates OpenSSL through Homebrew on Mac or pkg-config on
Linux. Build output belongs in `build/`, already ignored by the starter's Git
configuration. Keep runtime databases there, not among tracked source files.

## Start your own local application

First run, from the project root:

```bash
./build/mentor_api --init --database build/mentoring.db --admin-email admin@example.test
```

Choose your own administrator email if desired. The program asks twice for
a password of at least 12 bytes; terminal input is hidden. Passwords are not
command-line arguments. Bootstrap works only before any credential exists,
so this command cannot reset an initialized administrator.

Expected startup message:

```text
Administrator created.
Mentoring API listening on http://127.0.0.1:8080
```

In a second terminal:

```bash
curl http://127.0.0.1:8080/api/health
```

Expected JSON: `{"schema_version":2,"status":"ok"}` (key order is irrelevant).
Use Control+C in the server terminal to stop it. Restart later without
bootstrap arguments:

```bash
./build/mentor_api --database build/mentoring.db
```

Optional CLI arguments: `--port PORT`, `--project-root PATH`, `--init`,
`--migrate-only`, `--database PATH`, `--admin-email EMAIL`. Run from the project
root unless you supply `--project-root`. The database's parent directory must exist.

No frontend is included in this phase. `API.md` is the frontend contract;
the integration tests provide a complete working example client sequence.

## Existing Phase 4 database

Migration 002 adds credentials, hashed sessions, run kind, and a source-run
reference for rechecks. Existing users, preferences, assignments, and
verification history are preserved. Test the migration against a backup copy
of a persistent database first. The integration suite covers a version-1
fixture upgrading to version 2 without losing its assignments.

To apply pending migrations without serving HTTP:

```bash
./build/mentor_api --init --migrate-only --database build/your-database-copy.db
```

`--init` applies 001 only to an empty unversioned database, then 002 only to
version 1. Version 2 is left alone; unsupported versions fail startup.
Existing Phase 4 users have no password credential. Bootstrap can initialize
an existing active admin with the same email when the credentials table is
empty. This version provides initial credentials through admin-created users;
adding credentials to other previously imported users is not an API feature yet.

## CMake route (also verified in Linux)

The API is optional so earlier core builds still do not require OpenSSL:

```bash
cmake -S . -B build/cmake-api -DMENTOR_BUILD_API=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake-api
ctest --test-dir build/cmake-api --output-on-failure
```

On Mac, if CMake cannot locate Homebrew OpenSSL, configure with:

```bash
cmake -S . -B build/cmake-api -DMENTOR_BUILD_API=ON -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
```

The executable is `build/cmake-api/mentor_api`. If Python is discovered, CTest
runs five suites: matching smoke tests, verifier tests, Phase 3 validation,
HTTP API integration, and database schema tests. Without Python it runs the
three C++ suites; the HTTP tests then need Python separately.

## Authentication and roles

Administrators create accounts; open self-registration is not provided.
Login returns a random bearer token, valid for eight hours. Clients send
`Authorization: Bearer <token>`. Logout deletes that session. Changing a
password revokes ALL of that user's sessions. Deactivation revokes sessions
and prevents login. An admin cannot deactivate their own account through
this endpoint. Admin privileges are checked from the current users record.

Passwords use salted scrypt through OpenSSL: N=32768, r=8, p=3, 16-byte random
salt, 32-byte derived hash. Sessions store only a SHA-256 token digest, not
the bearer token. Password comparison uses constant-time byte comparison.
The fixed password scheme parameters belong to schema/API version 2; changing
them requires a credential-version migration, not silently editing constants.

Login attempts are limited to 20 per source IP in a rolling 60-second window.
The limiter is in memory and resets on restart; local users share the same
loopback source IP. It is not a distributed production limiter.

The account identified by a token owns preference submissions and ratings.
Caller-supplied owner/author IDs cannot impersonate a different participant.
Non-admin participants see only their own assignments and never rating data.
Admin-only routes include user management, roster changes, matching/rechecking,
readiness, matching history, and rating reads.

## Transactions and real matching

A matching request acquires a single `BEGIN IMMEDIATE` write transaction.
It reads the newest submitted preference versions for the frozen roster,
maps permanent IDs to zero-based indices, calls the existing `matchMentors`,
calls `verifyStability`, maps IDs back, stores snapshots/assignments/evidence,
and finalizes the run. Only then does it commit. Exceptions roll back.

Rechecks instead load the source run's existing pairs and verify those pairs
against the latest submitted rankings. They do not rematch. The new run has
kind `recheck`, a `source_run_id`, and zero proposals. An unstable recheck
is saved as rejected, with permanent user IDs in its blocking-pair witness.
The original run remains historical evidence. Ratings cannot target rejected runs.

This connects the actual C++ core to SQLite; it replaces Phase 4's fixture-only
behavior for real requests. The standalone Phase 4 demo remains a fixture.

## Request processing and current limits

`server.cpp` handles HTTP, JSON parsing, CORS, and error responses.
`service.cpp` authenticates callers, checks roles, and executes operations.
`database.cpp` owns SQLite handles/statements and transactions.
`auth.cpp` owns random generation and password/token hashing.

One database connection is protected by a mutex, serializing operations. This
keeps transaction ownership understandable for the class project. Password
hashing also runs within that lock, so this is not a high-throughput server.
New database/journal files are created with restrictive process permissions.

The local server listens only on 127.0.0.1. JSON bodies are limited to 64 KiB
and 32 nesting levels. Rounds are limited to 2,000 total participants in this
first API implementation. HTTP read/write timeouts are ten seconds. These
limits are implementation defaults, not changes to the matching theorem.

By default cross-origin browser requests are refused. For a separate local
frontend, set ONE exact origin before starting the server, for example:

```bash
MENTOR_ALLOWED_ORIGIN=http://localhost:5173 ./build/mentor_api --database build/mentoring.db
```

Bearer auth is sent explicitly; no auth cookie is used. Frontend token
storage/refresh behavior will be designed during UI integration. Public
network binding and HTTPS deployment are intentionally not implemented here;
this artifact is for localhost development. Password recovery and provisioning
credentials for old non-admin imported accounts remain future work.

## Review questions — collected at the end

1. How do HTTP routes differ from the pure matching functions?
2. Why must the token determine the preference owner rather than a JSON user ID?
3. How are salts, password hashes, bearer tokens, and token digests different?
4. Why does password change revoke existing sessions?
5. What does RAII do for SQLite statements and failed transactions?
6. Why is matching and snapshot storage done in one transaction?
7. Why translate stable database IDs before calling Gale–Shapley?
8. How is a recheck different from a rematch?
9. Why may a 201 response contain a rejected recheck?
10. Why must current-input freshness remain separate from historical verification?
11. How do integration tests prove authorization restrictions as well as success paths?
12. What are the tradeoffs of serializing requests through one connection?

## References

- https://github.com/yhirose/cpp-httplib
- https://json.nlohmann.me/features/parsing/parse_exceptions/
- https://www.sqlite.org/c3ref/prepare.html
- https://www.sqlite.org/foreignkeys.html
- https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html
- https://formulae.brew.sh/formula/openssl%403
