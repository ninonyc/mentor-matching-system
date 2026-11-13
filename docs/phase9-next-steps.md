# Immediate next step

Render is connected. Before it can build this release, the deployment files
must be present in the project's GitHub repository. The connected Render
account was not used to create a service during this preparation session.

1. Extract this update and copy its contents into the existing project root.
   Include Dockerfile, .dockerignore, render.yaml and deployment/ as well as
   the updated frontend files and controller test.
2. In VS Code Source Control, review and commit the project source changes,
   then push main to https://github.com/ninonyc/mentor-matching-system.
   Keep build/ databases, accounts.json, .env and passwords out of the commit.
3. Once that push is available, create/review the Render Docker Blueprint.
   Its persistent SQLite disk requires a paid service; review the plan charge
   before creating it. No payment authorization is presumed from this package.
4. Enter a new production administrator email and password in Render's private
   environment settings, not in chat or Git. The Blueprint prompts for them.
5. Continue until Docker build, /api/health over HTTPS and first login are
   verified. Only then is deployment complete.

Full details, limitations and primary references: phase9-deployment.md.
