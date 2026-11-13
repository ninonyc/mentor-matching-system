# Phase 7 — Live integration

The UI calls the C++ API, which reads and writes SQLite and calls the existing
matcher and independent verifier. Sample-data mode bypasses this path; do not
use it for integration checks.

## Install and run the automated HTTP workflow

Copy this update into the existing project root. It includes the restart fix in `backend/src/server.cpp`, launcher tools,
integration tests, and Phase 7 documentation. No application database or
credential is included. Replace the included files, then rebuild the backend. Keep the latest Phase 6 frontend installed.

```bash
bash backend/build.sh
python3 backend/tests/test_api.py
python3 tests/integration/test_live_workflow.py
```

Expected: 34 API tests and three integration tests, both ending with `OK`.
The workflow test starts its own backend and static server on free ports, uses
a temporary database, submits all five preference lists, computes two stable
pairs, submits two ratings, checks rating access, restarts the server, and
checks preserved runs, feedback, and SQLite integrity. It also verifies that
setup refuses to overwrite an existing cohort. This is an HTTP workflow test,
not a browser test. It does not change your application database.

## Start the live browser cohort

Stop any backend on port 8080 or frontend on port 5173 first. The launcher
refuses busy ports; it does not stop existing servers.

```bash
python3 tools/phase7_demo.py
```

Open http://127.0.0.1:5173. The launcher starts both servers and creates a
separate database at `build/phase7-demo/mentoring.db`. Credentials are in
`build/phase7-demo/accounts.json`, readable by the creating user. They are
randomly generated fictional test credentials; do not reuse them for real
accounts. Keep this ignored build directory out of commits and screenshots.

The roster is frozen; no preferences or assignments are seeded. Each person
must submit preferences through the real UI. All accounts share the generated
test password for this isolated cohort.

| Entrance | Account | Name |
| --- | --- | --- |
| Administrator, bottom right | admin@ascent.test | Administrator |
| Mentor | alex@ascent.test | Alex Rivera |
| Mentor | morgan@ascent.test | Morgan Chen |
| Mentor | sam@ascent.test | Sam Taylor |
| Mentee | jordan@ascent.test | Jordan Ellis |
| Mentee | casey@ascent.test | Casey Brooks |

Use **Sign in** with these accounts. Do not select sample-data mode.
Stop both servers with Ctrl+C in the launcher terminal. Resume this exact
cohort later with:

```bash
python3 tools/phase7_demo.py --resume
```

For a new empty cohort, choose a new directory; no existing directory is overwritten:

```bash
python3 tools/phase7_demo.py --directory build/phase7-demo-2
```

## Manual browser checks

Record observed results, rather than marking these passed from expected behavior.

| ID | Action | Expected |
| --- | --- | --- |
| P7-M01 | Open all three entrances; leave each using ×, Back, or Escape. | Dialog closes; mountain remains. Instruction is below the start of “Rise together,” darker than the headline. Both figures lift on hover; summit admin button is absent. |
| P7-M02 | Sign in as admin before any submissions. | Both preference groups visible; Assign disabled; five outstanding lists. Exit returns to mountain. |
| P7-M03 | Sign in as each of the five participants. Try arrow reorder and drag/drop, restore original order, then submit. | Complete lists save; no demo banner; sign in again to see saved order and a change-explanation field. |
| P7-M04 | Sign in as admin after all submissions. Expand lists. Open assignment confirmation and cancel using ×, Back, and Escape. | Assign enabled; rankings visible. Cancellations create no run. |
| P7-M05 | Confirm one assignment. | Two verified pairs shown. With all default orders unchanged: Alex–Jordan, Morgan–Casey; Sam is unassigned. Different orders may produce different stable pairs. |
| P7-M06 | Sign in as Jordan, then Casey; submit feedback from each. Sign in as Sam. | Each mentee sees their own partner and can submit feedback. Sam sees “currently unassigned.” |
| P7-M07 | Stop launcher, resume, and sign in again. | Submitted rankings and assignments remain. A fresh browser login is required because tokens live in memory. |
| P7-M08 | With a saved list, reorder and try to update with no explanation; then provide an explanation. | Empty explanation is rejected; explained update saves. Original matching is historical and marked outdated when read again. |

Keep screenshots in `docs/screenshots/` using these IDs. Avoid capturing passwords
or tokens. Admin ratings, recheck, history and cohort-management screens are
still absent; those operations remain accessible through the existing API.

## Optional automated browser workflow

Node and Playwright are optional; the manual browser workflow above needs
neither. If you want repeatable browser checks, install Playwright into the
ignored build directory:

```bash
npm install --prefix build/phase7-browser --no-save playwright
node build/phase7-browser/node_modules/playwright/cli.js install chromium
node tests/integration/browser_workflow.cjs
```

The runner starts temporary backend/frontend fixtures at 8080/5173, drives
real logins, dismisses dialogs, checks alignment and hover lift, submits all
rankings, cancels matching, creates a verified run, reads assignments, submits
feedback, checks the unassigned mentor and admin rating records, then cleans
up its temporary database. Expected final line:

```text
PHASE 7 BROWSER WORKFLOW PASSED
```

Set `PYTHON` if your Python command is not `python3`. An existing compatible
Chromium may be selected with `PLAYWRIGHT_CHROMIUM_EXECUTABLE`; normally let
Playwright use its installed browser. This browser runner is provided but has
not passed execution in the authoring environment: browser launch is blocked.
A syntax check does not establish browser success. Your local run remains pending.

## Review questions

1. Why does sample-data mode not verify integration?
2. Which component determines participant identity and matching permissions?
3. Why does cancelling the assignment dialog need to make no matching request?
4. What does restarting the server prove about SQLite persistence?
5. Why are outdated inputs different from an unstable result?

## Restart correction

The original launcher could report stopped servers as busy because its port
probe did not permit reuse after closed connections. The updated probe and
backend use SO_REUSEADDR; the backend no longer uses the HTTP library default
SO_REUSEPORT. An active listener is still rejected, with the exact port and
inspection command shown. Tests now exercise immediate restart on the same
ports, recently closed sockets, and rejection of an active listener.

Replace `tools/phase7_demo.py` and `backend/src/server.cpp`, rebuild, then
resume your existing cohort. There is no need to delete or recreate its database.
