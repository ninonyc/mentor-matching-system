# Phase 9 — Deployment preparation

The author confirmed all four additional browser checks passed: login cancellation,
matching cancellation, wrong-password rejection/recovery, and missing-explanation
preference rejection with original order restored. These are recorded as manual
passes, not an automated browser run. Broader connection-recovery and cross-round
browser cases remain unreported, as previously documented. The author asked to
proceed to deployment.

Render was selected because the required C++ server needs a native/container
runtime and SQLite needs persistent filesystem storage. The user successfully
connected Render. No Render service-management tool or skill appeared in this
session's inventory after connection; deployment actions were not invoked.
No service, disk, charge, source push, or public URL is claimed.

Prepared multi-stage Docker build, Caddy static/API routing, a Render Blueprint
with one paid instance and a persistent 1 GB disk, runtime administrator bootstrap,
pre-migration SQLite backups and coordinated process shutdown. The runtime drops
to UID/GID 10001 before serving. No local test DB or accounts are packaged.

Updated frontend to use the same origin's /api in hosting/container environments
while preserving separate loopback API for ports 5173/4173 in local development.
Added code-native favicon and a regression check for hosted/container routing.

Verification executed in Linux:

| Check | Result |
| --- | --- |
| Frontend controller | PASS: seven checks (six existing plus hosted-origin check) |
| Deployment helpers | PASS: six tests, 0.422 seconds in recorded run |
| Real C++ bootstrap | Tested in temporary DB; repeated startup preserved credentials |
| SQLite backup | Data/integrity/0600 permissions checked; failed output cleaned up |
| Docker image build | NOT_RUN: Docker unavailable |
| Caddy validation | Encoded in Docker build; NOT_RUN locally |
| Render Docker build / live HTTPS | NOT_RUN: source update and hosted service creation pending |

Repository URL was retrieved from earlier user-provided project context:
https://github.com/ninonyc/mentor-matching-system. Remote content verification
was blocked in the authoring environment, so the user's current remote source
has not been compared with the tested workspace source. The update must be
committed/pushed before Render can build it.

The user's bootstrap password must be entered only as a private Render runtime
setting; no secret is embedded in Dockerfile, Blueprint, logs, or archive.
This is deployment preparation, not proof of an operational hosted system.
