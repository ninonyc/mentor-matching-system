# Phase 6 — Ascent mountain interface

Ascent is a provisional visual name. This is plain HTML, CSS, and JavaScript:
no Node installation, bundler, or frontend framework is required.

Copy the update into your project root, preserving paths. Replace the two
included backend files to add the admin-only preference overview. Existing
logs, screenshots, database migrations and CMake configuration are excluded.

## Try the design first

From your project root:

```bash
python3 -m http.server 5173 --bind 127.0.0.1 --directory frontend
```

Open http://127.0.0.1:5173 in your browser. Click a person or the background
summit, then choose **Explore this role with sample data**. No account or
backend is needed for this explicitly labeled demo mode. Demo assignments
are illustrative and do not run Gale–Shapley. Nothing is saved in demo mode.

The lower person is the mentee; the upper person is the mentor. The summit
and the footer Administrator link open the same administrator login.
After a mentor enters, the mentee gives way to mentee rankings; the reverse
happens for mentees. Administrators see both groups and expandable rankings.

## Use your actual backend

Rebuild it after copying the update:

```bash
bash backend/build.sh
python3 backend/tests/test_api.py
```

The API suite now has 34 tests, including the new admin-only overview.
Then start your existing initialized application database:

```bash
MENTOR_ALLOWED_ORIGIN=http://127.0.0.1:5173 ./build/mentor_api --database build/mentoring.db
```

If you have never initialized an application database/admin account, follow
the first-run instructions in `backend/README.md` with the same origin
environment variable. Test-suite databases are temporary and cannot be used
for normal login. Do not use the sample-data button to test real login.

Keep the static frontend server running in another terminal and open
http://127.0.0.1:5173 (not localhost, unless you also configure that exact
origin). Sign in with a real application account. Account privileges and
round-specific roles come from the server; choosing a mountain entrance
does not grant permissions.

Live cohort setup (creating accounts, rounds, roster, freezing) currently
uses the API. These management forms are not included in this design pass.
An account without enrolled rounds gets an explicit empty state.

## Current behavior

- Tokens stay in JavaScript memory, not localStorage. Reloading requires login.
- Preferences support HTML drag/drop and labeled up/down buttons. Mobile
  users can use the buttons; layout stacks at narrow viewport widths.
- A changed submitted ranking requires a written reason before resubmission.
- Live matching calls the C++ matching/verification endpoint. The button is
  disabled until the frozen round has everyone's submitted rankings.
- Assigned users can submit private ratings through the existing API.
- Admin preference overview is GET `/api/rounds/{round}/preferences-overview`.
  It returns each participant's latest targets, or null when none are submitted,
  plus readiness and missing count. Non-admin access is forbidden.
- Ratings read/history/recheck/user-management screens remain future UI work.
- Full live browser end-to-end testing belongs to Phase 7 integration.

## Files and review questions

`index.html` defines the scene, entrances, dialog and workspace containers.
`styles.css` controls illustration/layout/responsiveness. `app.js` owns UI state,
DOM updates, sample data, and fetch calls to the existing API. The landscape
is an editable inline SVG rather than a generated raster background.

Review questions, collected at the end:
1. Why must account permissions come from the API rather than the entrance clicked?
2. How does rendering depend on the current role, round and preference state?
3. How do array reordering and DOM rendering work together?
4. Why distinguish sample data from real API results?
5. Why are names escaped before being placed in HTML?
6. What does an outdated assignment mean, compared with an unstable assignment?
