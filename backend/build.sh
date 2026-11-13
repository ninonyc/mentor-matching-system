#!/usr/bin/env bash
set -euo pipefail
# Run from the repository root. Dependencies other than OpenSSL are vendored.
mkdir -p build
crypto_flags=()
if command -v brew >/dev/null 2>&1; then
    openssl_prefix="$(brew --prefix openssl@3)"
    crypto_flags=("-I${openssl_prefix}/include" "-L${openssl_prefix}/lib")
elif command -v pkg-config >/dev/null 2>&1 && pkg-config --exists openssl; then
    read -r -a crypto_flags <<< "$(pkg-config --cflags --libs openssl)"
else
    echo 'OpenSSL 3 development headers are required. On macOS: brew install openssl@3' >&2
    exit 1
fi
"${CC:-cc}" -O2 -DSQLITE_THREADSAFE=1 -c backend/third_party/sqlite3.c -o build/sqlite3.o
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread \
    -Ialgorithm/include -Ibackend/include -isystem backend/third_party \
    "${crypto_flags[@]}" \
    algorithm/src/matching.cpp algorithm/src/stability.cpp \
    backend/src/database.cpp backend/src/auth.cpp backend/src/service.cpp backend/src/server.cpp \
    build/sqlite3.o -o build/mentor_api -lcrypto
