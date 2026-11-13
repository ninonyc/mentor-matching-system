# HTTP contract — API v1, database schema v2

Base URL: `http://127.0.0.1:8080`. Routes use JSON objects and return JSON.
Send `Content-Type: application/json` for bodies. Every route except health
and login requires `Authorization: Bearer <token>`.

## Routes

| Method / path | Access | Request fields / result |
| --- | --- | --- |
| GET `/api/health` | Public | status, schema_version. |
| POST `/api/auth/login` | Public, rate-limited | email, password -> token, token_type, expires_at (Unix seconds). |
| POST `/api/auth/logout` | Authenticated | Revoke the current token. |
| GET `/api/me` | Authenticated | id, name, email, is_admin. |
| POST `/api/me/password` | Authenticated | current_password, new_password; revokes all sessions. |
| POST `/api/users` | Admin | name, email, password, optional is_admin boolean. |
| GET `/api/users` | Admin | users array; never credentials or sessions. |
| PATCH `/api/users/{user}` | Admin | active boolean; cannot target own active status. |
| POST `/api/rounds` | Admin | name -> collecting round. |
| GET `/api/rounds` | Authenticated | Admin sees all rounds; others see participating rounds only. |
| POST `/api/rounds/{round}/participants` | Admin | user_id, role (`mentor` or `mentee`); collecting only. |
| DELETE `/api/rounds/{round}/participants/{user}` | Admin | Remove collecting-round participation. |
| GET `/api/rounds/{round}/participants` | Admin or member | Public roster within the round: ID/name/role, not email or rankings. |
| POST `/api/rounds/{round}/freeze` | Admin | Freeze roster. |
| POST `/api/rounds/{round}/close` | Admin | Close round. |
| GET `/api/rounds/{round}/readiness` | Admin | mentors, mentees, missing_preferences user IDs, ready boolean. |
| PUT `/api/rounds/{round}/preferences` | Round member | targets array of opposite-side user IDs, optional reason; always the caller's list. |
| GET `/api/rounds/{round}/preferences` | Round member | Caller's latest submitted id/revision/reason/targets. |
| POST `/api/rounds/{round}/matches` | Admin | Compute/verify/save a new matching from current submitted preferences. |
| POST `/api/rounds/{round}/rechecks` | Admin | run_id of a verified source in the same round; reverify existing pairs with latest preferences. |
| GET `/api/rounds/{round}/matches` | Admin | runs array with assignment/verification details. |
| GET `/api/rounds/{round}/my-assignment` | Admin or member | Latest verified run; non-admins receive only their own pair, or empty assignments when unmatched. |
| GET `/api/matches/{run}` | Admin or round member | Non-admins receive only their own pair and only for verified runs. |
| POST `/api/ratings` | Assigned partner | assignment_id, score integer 1–5, optional comment. Author/target derived server-side. |
| GET `/api/ratings` | Admin | ratings array, including comments. |

## Preference request

```json
{"targets":[40,50],"reason":"Changed after meeting the participants"}
```

IDs are permanent user IDs, NOT indices or numerical scores. Array order
defines best-to-worst ranking. The first submission can omit reason; after
a previous submission, a nonblank reason is required. Submission is atomic:
incomplete lists do not leave partial versions behind. No draft-editing
endpoint is exposed yet; PUT saves a complete new submitted version.

## Matching run response

```json
{
  "id":1,"round_id":1,"state":"verified","kind":"matching",
  "source_run_id":null,"inputs_are_current":true,"proposal_count":4,
  "verification":{"status":"Stable","explanation":"Valid one-to-one assignment; no blocking pair exists"},
  "assignments":[
    {"id":1,"mentor_id":10,"mentee_id":40},
    {"id":2,"mentor_id":20,"mentee_id":50}
  ]
}
```

These IDs illustrate the Phase 4 fixture; actual IDs are database-generated.
An unmatched mentor has no assignment row. Non-admin responses omit verification
and proposal_count and filter assignments to pairs involving the caller.

`inputs_are_current=false` means newer submitted preferences exist. It does
NOT mean unstable. `state=verified` records the status for that run's stored
inputs. Do not present outdated inputs as freshly verified. The endpoint
`my-assignment` finds the latest historically verified run, even if outdated.

POST matching/recheck returns 201 because it creates an audit record. A
rejected recheck is a validly created record, not an accepted assignment.
Clients must inspect `state` and admin `verification.status`. Recheck status
can be Stable or Unstable; a blocking_pair uses permanent user IDs.
A recheck has kind `recheck`, references source_run_id, and makes zero proposals.

## Errors and limits

Errors have shape `{"error":"explanation"}`.

| Status | Meaning |
| --- | --- |
| 200 | Read/update/logout successful. |
| 201 | User, round, participant, preference version, run, or rating created. |
| 204 | Allowed CORS preflight. |
| 400 | Invalid JSON/types, IDs, duplicate target, or field limits. |
| 401 | Missing/expired token, wrong password, inactive account, or bad current password. |
| 403 | Role/ownership/round membership restriction or disallowed browser origin. |
| 404 | Missing resource/endpoint, no own preferences, or no verified assignment yet. |
| 409 | Database/lifecycle constraint: incomplete rankings, missing change reason, duplicate rating, etc. |
| 413 | Body exceeds 64 KiB. |
| 415 | Non-JSON content type for a nonempty body. |
| 429 | Login rate limit. |
| 500 | Unexpected internal failure; raw internal details are not returned. |
| 503 | Database busy/locked; retry later. |

No cookie session, pagination, email delivery, or browser UI is implemented.
Comment/reason max 2,000 bytes, name max 100, email max 254, password 12–256.
These checks are byte-length checks, not a full international email validator.
Only one exact browser origin can be configured; terminal clients need no Origin header.
