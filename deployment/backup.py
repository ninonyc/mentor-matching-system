#!/usr/bin/env python3
"""Make an application-consistent SQLite backup; never copy a live DB file."""
import argparse
from datetime import datetime, timezone
import os
from contextlib import closing
from pathlib import Path
import sqlite3


def backup(database, directory):
    database, directory = Path(database).resolve(), Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=True, mode=0o700)
    name = 'mentoring-' + datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ') + '.db'
    destination = directory / name
    # Restrictive permissions apply before SQLite writes credential hashes.
    fd = os.open(destination, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    os.close(fd)
    try:
        with closing(sqlite3.connect(database.as_uri() + '?mode=ro', uri=True)) as source:
            with closing(sqlite3.connect(destination)) as target:
                source.backup(target)
                if target.execute('PRAGMA integrity_check').fetchone()[0] != 'ok':
                    raise RuntimeError('Backup integrity check failed')
        return destination
    except BaseException:
        destination.unlink(missing_ok=True)
        raise


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--database', default='/var/data/mentoring.db')
    parser.add_argument('--directory', default='/var/data/backups')
    args = parser.parse_args()
    print(backup(args.database, args.directory))
