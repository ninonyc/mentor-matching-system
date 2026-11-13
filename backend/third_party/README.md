# Bundled dependencies

| Dependency | Pinned version | License |
| --- | --- | --- |
| cpp-httplib | v0.66.0 | MIT; see httplib-LICENSE. |
| nlohmann/json | v3.12.0 | MIT; see json-LICENSE. |
| SQLite amalgamation | 3.50.4 | Public domain; copyright/blessing appears in source headers. |

These upstream sources are included for reproducible builds without fetching
packages during CMake configuration. They are not original project code.
`sources.json` records download URLs and SHA-256 hashes (SQLite archive hash
is of the upstream ZIP). `files.sha256` records the actual bundled files.
Do not edit dependency internals as part of normal application work.
OpenSSL is a system dependency, not vendored here.

Official sources:
- https://github.com/yhirose/cpp-httplib
- https://github.com/nlohmann/json
- https://www.sqlite.org/2025/sqlite-amalgamation-3500400.zip
- https://www.sqlite.org/copyright.html
