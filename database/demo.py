"""Phase 4 schema demonstration; Python is a test utility, not the backend."""
from pathlib import Path
import argparse
import os
import sqlite3

SCHEMA = Path(__file__).parent / "migrations" / "001_initial.sql"


def connect(database=":memory:"):
    connection = sqlite3.connect(database)
    connection.execute("PRAGMA foreign_keys = ON")
    connection.execute("PRAGMA recursive_triggers = ON")
    connection.execute("PRAGMA busy_timeout = 5000")
    if connection.execute("PRAGMA foreign_keys").fetchone()[0] != 1:
        raise RuntimeError("SQLite foreign-key enforcement is required")
    return connection


def submit_preferences(db, owner, targets, revision=1, reason=None, round_id=1):
    version = db.execute(
        "INSERT INTO preference_versions(round_id,owner_id,revision,change_reason) VALUES(?,?,?,?)",
        (round_id, owner, revision, reason),
    ).lastrowid
    db.executemany(
        "INSERT INTO preference_entries(version_id,target_id,rank) VALUES(?,?,?)",
        [(version, target, rank) for rank, target in enumerate(targets, 1)],
    )
    db.execute("UPDATE preference_versions SET state='submitted' WHERE id=?", (version,))
    return version


def seed_demo(db):
    # Fictional data only. This fixture is known to be stable by inspection.
    # It demonstrates persistence; it does NOT invoke or replace the C++ verifier.
    with db:
        db.executemany(
            "INSERT INTO users(id,display_name,email,is_admin) VALUES(?,?,?,?)",
            [(1, "Demo Admin", "admin@example.test", 1),
             (10, "Mentor A", "a@example.test", 0),
             (20, "Mentor B", "b@example.test", 0),
             (30, "Mentor C", "c@example.test", 0),
             (40, "Mentee D", "d@example.test", 0),
             (50, "Mentee E", "e@example.test", 0)],
        )
        db.execute("INSERT INTO matching_rounds(id,name) VALUES(1,'Demo round')")
        db.executemany("INSERT INTO participants VALUES(1,?,?)",
                       [(10, "mentor"), (20, "mentor"), (30, "mentor"),
                        (40, "mentee"), (50, "mentee")])
        db.execute("UPDATE matching_rounds SET state='frozen' WHERE id=1")
        versions = {}
        for owner, targets in [(10, [40, 50]), (20, [50, 40]), (30, [40, 50]),
                               (40, [10, 30, 20]), (50, [20, 30, 10])]:
            versions[owner] = submit_preferences(db, owner, targets)
        db.execute("INSERT INTO matching_runs(id,round_id,requested_by) VALUES(1,1,1)")
        db.executemany("INSERT INTO run_preferences VALUES(1,1,?,?)", versions.items())
        db.executemany("INSERT INTO assignments(run_id,round_id,mentor_id,mentee_id) VALUES(1,1,?,?)",
                       [(10, 40), (20, 50)])
        db.execute("INSERT INTO verification_results(run_id,round_id,status,explanation) "
                   "VALUES(1,1,'Stable','Demo fixture: stable by inspection, not a C++ execution')")
        db.execute("UPDATE matching_runs SET state='verified',proposal_count=4 WHERE id=1")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", help="Create a NEW demo database; default: memory only")
    args = parser.parse_args()
    if args.database:
        # Refuse to overwrite an existing database, including one from an earlier demo.
        fd = os.open(args.database, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
        os.close(fd)
    db = connect(args.database or ":memory:")
    try:
        db.executescript(SCHEMA.read_text())
        seed_demo(db)
        print("SQLite schema version:", db.execute("PRAGMA user_version").fetchone()[0])
        print("Users:", db.execute("SELECT count(*) FROM users").fetchone()[0])
        print("Participants:", db.execute("SELECT count(*) FROM participants").fetchone()[0])
        print("Run state:", db.execute("SELECT state FROM matching_runs WHERE id=1").fetchone()[0])
        print("Assignments:", db.execute("SELECT count(*) FROM assignments").fetchone()[0])
        print("Inputs current:", bool(db.execute("SELECT inputs_are_current FROM run_freshness WHERE run_id=1").fetchone()[0]))
        issues = db.execute("PRAGMA foreign_key_check").fetchall()
        if issues:
            raise RuntimeError(str(issues))
        print("Foreign-key check: OK")
        print("DATABASE DEMO PASSED")
    finally:
        db.close()


if __name__ == "__main__":
    main()
