#!/usr/bin/env python3
"""Supervise the loopback C++ API and public Caddy router in one container."""
import os
from contextlib import closing
from pathlib import Path
import signal
import sqlite3
import subprocess
import sys
import time
import urllib.parse
import urllib.request
from backup import backup


def origin(value):
    parsed = urllib.parse.urlsplit(value)
    if (parsed.scheme not in ('http', 'https') or not parsed.netloc or parsed.username
            or parsed.password or parsed.path not in ('', '/') or parsed.query or parsed.fragment):
        raise RuntimeError('Set MENTOR_PUBLIC_ORIGIN to the exact public HTTPS origin')
    if parsed.scheme != 'https' and parsed.hostname not in ('127.0.0.1', 'localhost'):
        raise RuntimeError('Public deployment requires an HTTPS origin')
    return value.rstrip('/')


def initialize(binary, root, database, email, password):
    args = [str(binary), '--database', str(database), '--project-root', str(root)]
    # Back up any initialized existing database before applying numbered migrations.
    if database.exists() and database.stat().st_size:
        backup(database, database.parent / 'backups')
    subprocess.run(args + ['--init', '--migrate-only'], check=True,
                   stdin=subprocess.DEVNULL, timeout=60)
    with closing(sqlite3.connect(database)) as connection:
        credentials = connection.execute('SELECT count(*) FROM credentials').fetchone()[0]
    if credentials == 0:
        if not email or len(password.encode()) < 12 or '\n' in password or '\r' in password:
            raise RuntimeError('Fresh database needs bootstrap email and a password of at least 12 bytes')
        subprocess.run(args + ['--admin-email', email, '--migrate-only'],
                       input=password + '\n' + password + '\n', text=True, check=True, timeout=60)
    return args


def stop(children):
    for process in reversed(children):
        if process.poll() is None:
            process.terminate()
    for process in reversed(children):
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def main():
    os.umask(0o077)
    root = Path('/app')
    public_origin = origin(os.environ.get('MENTOR_PUBLIC_ORIGIN') or os.environ.get('RENDER_EXTERNAL_URL', ''))
    port = int(os.environ.get('PORT', '10000'))
    if port < 1024 or port > 65535 or port == 8080:
        raise RuntimeError('PORT must be 1024–65535 and distinct from internal API port 8080')
    data = Path(os.environ.get('MENTOR_DATA_DIR', '/var/data')).resolve()
    data.mkdir(parents=True, exist_ok=True, mode=0o700)
    if os.geteuid() == 0:
        # Only the dedicated volume directory and known runtime paths change ownership.
        for path in [data, data / 'backups', data / 'mentoring.db', data / 'mentoring.db-wal', data / 'mentoring.db-shm']:
            if path.exists():
                os.chown(path, 10001, 10001)
        os.setgroups([])
        os.setgid(10001)
        os.setuid(10001)
        os.environ['HOME'] = '/home/ascent'
    os.environ['PORT'] = str(port)
    os.environ['MENTOR_ALLOWED_ORIGIN'] = public_origin
    email = os.environ.pop('MENTOR_BOOTSTRAP_EMAIL', '')
    password = os.environ.pop('MENTOR_BOOTSTRAP_PASSWORD', '')
    args = initialize(root / 'build/mentor_api', root, data / 'mentoring.db', email, password)
    password = None
    children = []
    def interrupt(signum, frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, interrupt)
    signal.signal(signal.SIGINT, interrupt)
    try:
        children.append(subprocess.Popen(args + ['--port', '8080'], stdin=subprocess.DEVNULL))
        deadline = time.monotonic() + 30
        while True:
            if children[0].poll() is not None:
                raise RuntimeError('API stopped before health check succeeded')
            try:
                with urllib.request.urlopen('http://127.0.0.1:8080/api/health', timeout=2) as response:
                    if response.status == 200:
                        break
            except OSError:
                if time.monotonic() > deadline:
                    raise RuntimeError('API startup timed out')
                time.sleep(.1)
        children.append(subprocess.Popen(['caddy', 'run', '--config', str(root / 'deployment/Caddyfile'), '--adapter', 'caddyfile'], stdin=subprocess.DEVNULL))
        print('Ascent API and web router started.', flush=True)
        while True:
            for process in children:
                if process.poll() is not None:
                    raise RuntimeError('A supervised service exited; stopping container')
            time.sleep(.5)
    except KeyboardInterrupt:
        return 0
    finally:
        stop(children)


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        print('Startup failed: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
