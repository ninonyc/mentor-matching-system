#!/usr/bin/env python3
"""Start a separate local integration cohort. Application data is never reused."""
import argparse
import errno
import functools
import http.server
import json
import os
from pathlib import Path
import secrets
import socket
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
PEOPLE = [('Alex Rivera', 'alex', 'mentor'), ('Morgan Chen', 'morgan', 'mentor'),
          ('Sam Taylor', 'sam', 'mentor'), ('Jordan Ellis', 'jordan', 'mentee'),
          ('Casey Brooks', 'casey', 'mentee')]


def request(base, method, path, body=None, token=None):
    headers = {'Content-Type': 'application/json'}
    if token:
        headers['Authorization'] = 'Bearer ' + token
    req = urllib.request.Request(base + '/api' + path, method=method, headers=headers,
                                 data=json.dumps(body).encode() if body is not None else None)
    try:
        with urllib.request.urlopen(req, timeout=20) as response:
            return json.load(response)
    except urllib.error.HTTPError as error:
        # The caller still inspects the HTTP status, but the response is closed.
        error.close()
        raise


def available_port(port):
    with socket.socket() as listener:
        # Match the servers' restart behavior: recently closed connections may
        # remain in TIME_WAIT even though no process is listening.
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            listener.bind(('127.0.0.1', port))
            listener.listen(1)
        except OSError as error:
            if error.errno == errno.EADDRINUSE:
                raise RuntimeError(
                    f'Port {port} is unavailable. Check its listener with: '
                    f'lsof -nP -iTCP:{port} -sTCP:LISTEN. '
                    'Stop the old server in its terminal, then retry.'
                ) from error
            raise
        return listener.getsockname()[1]


class Fixture:
    def __init__(self, directory, api_port=8080, frontend_port=5173, resume=False):
        self.directory = Path(directory).resolve()
        self.api_port, self.frontend_port = api_port, frontend_port
        self.resume = resume
        self.server = self.http = self.thread = self.log = None
        self.base = 'http://127.0.0.1:' + str(api_port)
        self.origin = 'http://127.0.0.1:' + str(frontend_port)

    def start(self):
        binary = ROOT / 'build/mentor_api'
        if not binary.is_file():
            raise RuntimeError('Build the backend first: bash backend/build.sh')
        available_port(self.api_port)
        available_port(self.frontend_port)
        manifest = self.directory / 'accounts.json'
        if self.resume:
            if not manifest.is_file() or not (self.directory / 'mentoring.db').is_file():
                raise RuntimeError('Resume requires an existing phase7 directory and accounts.json')
            self.accounts = json.loads(manifest.read_text())
        else:
            self.directory.mkdir(mode=0o700, parents=True, exist_ok=False)
            password = secrets.token_urlsafe(20)
            self.accounts = {'password': password, 'admin': 'admin@ascent.test', 'participants': []}
        self.log = (self.directory / 'server.log').open('a')
        args = [str(binary), '--database', str(self.directory / 'mentoring.db'),
                '--project-root', str(ROOT), '--port', str(self.api_port)]
        if not self.resume:
            args += ['--init', '--admin-email', self.accounts['admin']]
        self.server = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=self.log,
                                       stderr=self.log, text=True,
                                       env=dict(os.environ, MENTOR_ALLOWED_ORIGIN=self.origin))
        try:
            if not self.resume:
                self.server.stdin.write((self.accounts['password'] + '\n') * 2)
            self.server.stdin.close()
            deadline = time.monotonic() + 30
            while True:
                if self.server.poll() is not None:
                    raise RuntimeError('Backend stopped; inspect ' + str(self.directory / 'server.log'))
                try:
                    request(self.base, 'GET', '/health')
                    break
                except (OSError, ValueError):
                    if time.monotonic() > deadline:
                        raise RuntimeError('Backend startup timed out')
                    time.sleep(.05)
            if not self.resume:
                self.seed()
                fd = os.open(manifest, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
                with os.fdopen(fd, 'w') as out:
                    json.dump(self.accounts, out, indent=2)
            handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(ROOT / 'frontend'))
            self.http = http.server.ThreadingHTTPServer(('127.0.0.1', self.frontend_port), handler)
            self.thread = threading.Thread(target=self.http.serve_forever, daemon=True)
            self.thread.start()
            return self
        except BaseException:
            self.close()
            raise

    def seed(self):
        token = request(self.base, 'POST', '/auth/login', {'email': self.accounts['admin'],
                        'password': self.accounts['password']})['token']
        round_id = request(self.base, 'POST', '/rounds', {'name': 'Phase 7 integration cohort'}, token)['id']
        self.accounts['round_id'] = round_id
        for name, slug, role in PEOPLE:
            email = slug + '@ascent.test'
            user = request(self.base, 'POST', '/users', {'name': name, 'email': email,
                           'password': self.accounts['password']}, token)
            request(self.base, 'POST', f'/rounds/{round_id}/participants',
                    {'user_id': user['id'], 'role': role}, token)
            self.accounts['participants'].append({'id': user['id'], 'name': name, 'email': email, 'role': role})
        request(self.base, 'POST', f'/rounds/{round_id}/freeze', {}, token)
        request(self.base, 'POST', '/auth/logout', {}, token)

    def close(self):
        if self.http:
            self.http.shutdown()
            self.http.server_close()
            self.http = None
        if self.server:
            if self.server.poll() is None:
                self.server.terminate()
                try:
                    self.server.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    self.server.kill()
                    self.server.wait()
            self.server = None
        if self.log:
            self.log.close()
            self.log = None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=ROOT / 'build/phase7-demo')
    parser.add_argument('--resume', action='store_true')
    parser.add_argument('--automated', action='store_true', help='Temporary fixture controlled by the browser runner')
    args = parser.parse_args()
    if args.automated and args.resume:
        parser.error('--automated and --resume cannot be combined')
    temp = tempfile.TemporaryDirectory() if args.automated else None
    fixture = Fixture(Path(temp.name) / 'cohort' if temp else args.directory, resume=args.resume)
    try:
        fixture.start()
        if args.automated:
            print(json.dumps({'origin': fixture.origin, 'base': fixture.base, **fixture.accounts}), flush=True)
            sys.stdin.readline()
        else:
            print('Live integration ready: ' + fixture.origin, flush=True)
            print('Login credentials: ' + str(fixture.directory / 'accounts.json'), flush=True)
            print('Use real Sign in, not sample-data mode. Stop both servers with Ctrl+C.', flush=True)
            while True:
                if fixture.server.poll() is not None:
                    raise RuntimeError('Backend exited unexpectedly; inspect server.log')
                time.sleep(.5)
    except KeyboardInterrupt:
        pass
    finally:
        fixture.close()
        if temp:
            temp.cleanup()


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        print('Integration setup failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
