"""Deployment startup/storage checks using the real C++ API, without Docker."""
from contextlib import closing
from pathlib import Path
import sqlite3
import stat
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'deployment'))
from start import initialize, origin, stop
from backup import backup


class DeploymentTests(unittest.TestCase):
    def test_origin_validation(self):
        self.assertEqual(origin('https://ascent.example/'), 'https://ascent.example')
        self.assertEqual(origin('http://localhost:10000'), 'http://localhost:10000')
        for value in ('', 'http://ascent.example', 'https://user:pass@ascent.example',
                      'https://ascent.example/path', 'https://ascent.example?token=secret'):
            with self.assertRaises(RuntimeError):
                origin(value)

    def test_real_bootstrap_and_repeat_does_not_reset_credentials(self):
        with tempfile.TemporaryDirectory() as tmp:
            db = Path(tmp) / 'mentoring.db'
            binary = ROOT / 'build/mentor_api'
            initialize(binary, ROOT, db, 'admin@deployment.test', 'Fictional-deployment-test!')
            with closing(sqlite3.connect(db)) as connection:
                old = connection.execute('SELECT * FROM credentials').fetchall()
                self.assertEqual(len(old), 1)
            initialize(binary, ROOT, db, 'different@deployment.test', 'Different-fictional-pass!')
            with closing(sqlite3.connect(db)) as connection:
                self.assertEqual(connection.execute('SELECT * FROM credentials').fetchall(), old)
            self.assertEqual(len(list((db.parent / 'backups').glob('*.db'))), 1)

    def test_fresh_database_requires_secret(self):
        with tempfile.TemporaryDirectory() as tmp:
            db = Path(tmp) / 'mentoring.db'
            with self.assertRaisesRegex(RuntimeError, 'bootstrap'):
                initialize(ROOT / 'build/mentor_api', ROOT, db, '', '')
            with closing(sqlite3.connect(db)) as connection:
                self.assertEqual(connection.execute('SELECT count(*) FROM credentials').fetchone()[0], 0)

    def test_backup_integrity_permissions_and_source_preserved(self):
        with tempfile.TemporaryDirectory() as tmp:
            db = Path(tmp) / 'test.db'
            with closing(sqlite3.connect(db)) as connection:
                connection.execute('CREATE TABLE evidence(value TEXT)')
                connection.execute("INSERT INTO evidence VALUES('kept')")
                connection.commit()
            result = backup(db, Path(tmp) / 'backups')
            with closing(sqlite3.connect(result)) as connection:
                self.assertEqual(connection.execute('PRAGMA integrity_check').fetchone()[0], 'ok')
                self.assertEqual(connection.execute('SELECT value FROM evidence').fetchone()[0], 'kept')
            self.assertEqual(stat.S_IMODE(result.stat().st_mode), 0o600)
            self.assertTrue(db.is_file())

    def test_failed_backup_removes_incomplete_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            destination = Path(tmp) / 'backups'
            with self.assertRaises(sqlite3.Error):
                backup(Path(tmp) / 'absent.db', destination)
            self.assertEqual(list(destination.glob('*.db')), [])

    def test_supervisor_stops_child_processes(self):
        child = subprocess.Popen([sys.executable, '-c', 'import time;time.sleep(60)'])
        try:
            stop([child])
            self.assertIsNotNone(child.poll())
        finally:
            if child.poll() is None:
                child.kill()
                child.wait()


if __name__ == '__main__':
    unittest.main(verbosity=2)
