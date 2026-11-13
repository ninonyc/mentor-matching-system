# Phase 9 — Deployment to Render

Status: source/configuration prepared; no hosted service or live URL verified.
Render connection was confirmed by the user. Its deployment actions were not
exposed in the tool inventory during this run, so no service was created.
The repository URL retrieved from earlier project context is
https://github.com/ninonyc/mentor-matching-system. Remote contents could not be
verified from this environment; the deployment update must be pushed first.

## Design

A single Docker service runs the existing C++ API on loopback port 8080 and
Caddy on Render's public PORT (default 10000). Caddy serves frontend files and
proxies /api routes without removing their prefix. Render terminates public
HTTPS. The frontend uses relative /api URLs when hosted, keeping bearer-token
requests on the same HTTPS origin. Static local development at ports 5173/4173
continues to use the separate loopback API as before.

SQLite is stored at /var/data/mentoring.db on a 1 GB persistent disk. This is
one service instance; a SQLite file is not shared across multiple containers.
Render requires a paid service for a persistent disk. Review the service and
disk charge in the Render dashboard before creating resources; no paid
resource was created by this update. No free-tier persistence is claimed.
A persistent disk means redeploys can cause a brief outage.

## Install the source update

Copy the archive contents into the existing repository, preserving paths.
New: Dockerfile, .dockerignore, render.yaml, deployment/ scripts/Caddyfile,
deployment tests, favicon, and Phase 9 documentation. Updated: frontend API
base selection, icon link, controller regression test. It does not include
local databases, test accounts, production credentials, .git metadata or build
outputs. The backend executable itself still builds from the existing C++.

Check the change list in VS Code. Commit the source update and push it to the
existing repository's main branch. Do not stage build/phase7-demo/accounts.json,
SQLite files, .env files, generated reports containing private data, or passwords.

The Dockerfile needs the complete project source in the repository; this ZIP
is an update, not a replacement for the repository. render.yaml points to the
known project URL and main branch. No remote push is claimed from this session.

## Verify locally before hosting

From the project root:

```bash
node --test tests/integration/frontend_restore.cjs
python3 tests/integration/test_deployment.py
```

Expected: seven controller checks and six deployment-helper checks pass. The
seventh controller test checks relative hosted/container API routing while
preserving the local development route. Deployment tests use a temporary DB
and the existing built C++ API; they do not touch the local mentoring cohort.

With Docker installed, optional full image validation:

```bash
docker build -t ascent-mentoring .
```

This runs the C++ build and Caddy config validation inside the image. Docker
was unavailable in the authoring environment, so successful image construction
is not claimed. Render must complete its Docker build before deployment can
be accepted. No local container or public HTTPS result has been verified.

## Create the service

After the update is pushed, use the Render connection or dashboard to create
a Blueprint from this repository and review render.yaml. The configuration is:

| Setting | Value |
| --- | --- |
| Repository | https://github.com/ninonyc/mentor-matching-system |
| Branch | main |
| Runtime | Docker |
| Dockerfile | ./Dockerfile |
| Compute | 0.5c-512mb (paid; review current price) |
| Persistent disk | 1 GB mounted at /var/data |
| Instances | 1 |
| Health check | /api/health |
| Automatic deploys | Off |

Enter MENTOR_BOOTSTRAP_EMAIL and MENTOR_BOOTSTRAP_PASSWORD as private runtime
settings in Render. Choose a new administrator email/password for this hosted
application; password must have at least 12 bytes and contain no newline.
Do not put the password into source, a command line, screenshots, chat, or a
Docker build argument. The Dockerfile never references secret build arguments.

The supervisor derives the exact allowed browser origin from Render's
RENDER_EXTERNAL_URL. For a later custom domain, set MENTOR_PUBLIC_ORIGIN to
its exact HTTPS origin and redeploy. The initial configuration intentionally
allows one exact origin, matching the C++ API's existing policy.

Before any public listener starts, the supervisor applies migrations, then
bootstraps only if the credentials table is empty. Password reaches the C++
bootstrap through stdin. Bootstrap secrets are removed from child environment
variables. Once credentials exist, future startups do not recreate/reset them.
After first success, remove MENTOR_BOOTSTRAP_PASSWORD from Render environment
settings and restart/redeploy; existing administrator credentials remain.

The container prepares only its dedicated volume/runtime paths and drops to
UID/GID 10001 before migrations, backend or public router execution. The API
remains bound to loopback within the container. Caddy is the public listener.

## Hosted acceptance

Do not report deployment complete until Render confirms the live service and
these checks have been performed against its exact returned URL:

1. GET /api/health returns status ok and schema_version 2 over HTTPS.
2. Landing page loads; browser API requests target that hosted origin, not
   127.0.0.1:8080. The favicon loads from the same origin.
3. New administrator can log in. A fresh hosted DB has no participants or
   rounds; the existing administrator-only API is used to provision a cohort.
4. Create participant accounts and a frozen round, submit preferences, run
   matching, and verify role-specific pairs and administrator-only feedback.
5. Restart/redeploy, sign in again, and confirm saved rankings, assignments
   and feedback remain on the persistent disk.
6. Remove bootstrap password configuration and confirm restart preserves login.

The local phase7 test cohort is not uploaded automatically. Production starts
with a new administrator to avoid publishing the shared fictional test password.
Provisioning participant accounts remains an API operation, as documented in
backend/API.md. Source deployment does not add account-management UI forms.

## Backups and rollback

Before migrating an existing initialized database at startup, the supervisor
makes a consistent SQLite backup under /var/data/backups using SQLite's backup
API. This includes private credential hashes and mentoring data; keep it private.
Additional backups may be made in the service shell:

```bash
python3 /app/deployment/backup.py
```

Backups have 0600 permissions and are integrity-checked. Startup backups share
the service disk and are not offsite disaster recovery. Retention is not automated;
monitor disk use and export private backups to your chosen secure backup storage.
Do not copy a live SQLite file directly or treat a generic disk snapshot as a
verified database backup. Test restore procedures before real-data use.

Code rollback and database rollback are separate. Do not overwrite a live
SQLite DB during service operation. Stop the service, preserve the current DB,
validate the intended backup and schema compatibility, then restore through
a deliberate maintenance procedure. No destructive restore has been performed.

## Current class-project limits

The API's in-memory login limiter sees the loopback reverse proxy, so attempts
share its existing 20-per-minute limit. This is adequate for a small sequential
demonstration, not a claim of scalable per-user throttling. User-management,
full history and recheck screens remain API-only. Password recovery is absent.
No production load test, API sanitizer test, multi-instance SQLite deployment,
offsite backup schedule, or complete hosted browser test is claimed here.

## Primary references consulted

- https://render.com/docs/docker
- https://render.com/docs/disks
- https://render.com/docs/blueprint-spec
- https://render.com/docs/compute-plans
- https://render.com/docs/environment-variables
- https://caddyserver.com/docs/caddyfile/directives/handle
- https://caddyserver.com/docs/caddyfile/directives/reverse_proxy
