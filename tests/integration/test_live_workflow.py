"""Real HTTP/SQLite workflow; browser interaction is tested separately."""
import importlib.util
from pathlib import Path
import sqlite3
import socket
import tempfile
import unittest
import urllib.error

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('phase7_demo', ROOT / 'tools/phase7_demo.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PortChecks(unittest.TestCase):
    def test_active_listener_reports_exact_port(self):
        with socket.socket() as listener:
            listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            listener.bind(('127.0.0.1', 0))
            listener.listen(1)
            port = listener.getsockname()[1]
            with self.assertRaisesRegex(RuntimeError, f'Port {port} is unavailable'):
                module.available_port(port)

    def test_recently_closed_connection_can_restart(self):
        with socket.socket() as listener:
            listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            listener.bind(('127.0.0.1', 0))
            listener.listen(1)
            port = listener.getsockname()[1]
            with socket.create_connection(('127.0.0.1', port)) as client:
                peer, _ = listener.accept()
                with peer:
                    peer.shutdown(socket.SHUT_WR)
                    self.assertEqual(client.recv(1), b'')
        self.assertEqual(module.available_port(port), port)


class LiveWorkflow(unittest.TestCase):
    def test_persistent_cohort_matching_feedback_and_restart(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary) / 'cohort'
            fixture = module.Fixture(directory, module.available_port(0), module.available_port(0))
            try:
                fixture.start()
                accounts = fixture.accounts
                password = accounts['password']
                call = lambda method, path, body=None, token=None: module.request(fixture.base, method, path, body, token)
                login = lambda email: call('POST', '/auth/login', {'email': email, 'password': password})['token']
                admin = login(accounts['admin'])
                prefix = '/rounds/' + str(accounts['round_id'])
                self.assertFalse(call('GET', prefix + '/readiness', token=admin)['ready'])
                people = accounts['participants']
                tokens = {}
                for person in people:
                    token = tokens[person['id']] = login(person['email'])
                    targets = [other['id'] for other in people if other['role'] != person['role']]
                    call('PUT', prefix + '/preferences', {'targets': targets}, token)
                self.assertTrue(call('GET', prefix + '/readiness', token=admin)['ready'])
                run = call('POST', prefix + '/matches', {}, admin)
                self.assertEqual(run['state'], 'verified')
                self.assertEqual(run['verification']['status'], 'Stable')
                self.assertEqual(len(run['assignments']), 2)
                mentees = [p for p in people if p['role'] == 'mentee']
                for person in mentees:
                    mine = call('GET', prefix + '/my-assignment', token=tokens[person['id']])
                    self.assertEqual(len(mine['assignments']), 1)
                    pair = mine['assignments'][0]
                    self.assertEqual(pair['mentee_id'], person['id'])
                    call('POST', '/ratings', {'assignment_id': pair['id'], 'score': 5,
                         'comment': 'Phase 7 live integration'}, tokens[person['id']])
                self.assertEqual(len(call('GET', '/ratings', token=admin)['ratings']), 2)
                with self.assertRaises(urllib.error.HTTPError) as denied:
                    call('GET', '/ratings', token=tokens[mentees[0]['id']])
                self.assertEqual(denied.exception.code, 403)
                with urllib.request.urlopen(fixture.origin + '/app.js') as response:
                    self.assertIn(b'http://127.0.0.1:8080/api', response.read())
                fixture.close()
                resumed = module.Fixture(directory, fixture.api_port, fixture.frontend_port, resume=True)
                fixture = resumed.start()
                admin = login(accounts['admin'])
                self.assertEqual(len(call('GET', prefix + '/matches', token=admin)['runs']), 1)
                self.assertEqual(len(call('GET', '/ratings', token=admin)['ratings']), 2)
                with sqlite3.connect(directory / 'mentoring.db') as database:
                    self.assertEqual(database.execute('PRAGMA integrity_check').fetchone()[0], 'ok')
                # New setup never overwrites this persisted cohort.
                duplicate = module.Fixture(directory, module.available_port(0), module.available_port(0))
                with self.assertRaises(FileExistsError):
                    duplicate.start()
            finally:
                fixture.close()


if __name__ == '__main__':
    unittest.main(verbosity=2)
