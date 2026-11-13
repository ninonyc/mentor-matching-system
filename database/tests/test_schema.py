"""Database invariants only; no substitute for matching/stability tests."""
from pathlib import Path
import sqlite3
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from demo import SCHEMA, connect, seed_demo, submit_preferences


class SchemaTests(unittest.TestCase):
    def setUp(self):
        self.db = connect()
        self.db.executescript(SCHEMA.read_text())
        seed_demo(self.db)

    def tearDown(self):
        self.db.close()

    def rejects(self, sql, params=()):
        self.db.execute("SAVEPOINT rejection_check")
        try:
            with self.assertRaises(sqlite3.IntegrityError):
                self.db.execute(sql, params)
        finally:
            self.db.execute("ROLLBACK TO rejection_check")
            self.db.execute("RELEASE rejection_check")

    def new_run(self):
        self.db.execute("INSERT INTO matching_runs(id,round_id,requested_by) VALUES(2,1,1)")
        self.db.execute("INSERT INTO run_preferences SELECT 2,round_id,user_id,version_id "
                        "FROM run_preferences WHERE run_id=1")

    def draft(self, reason="Updated preference after meeting participants"):
        return self.db.execute("INSERT INTO preference_versions(round_id,owner_id,revision,change_reason) "
                               "VALUES(1,10,2,?)", (reason,)).lastrowid

    def test_initial_fixture(self):
        self.assertEqual(self.db.execute("PRAGMA user_version").fetchone()[0], 1)
        self.assertEqual(self.db.execute("PRAGMA foreign_key_check").fetchall(), [])
        self.assertEqual(self.db.execute("SELECT state FROM matching_runs").fetchone()[0], "verified")

    def test_missing_user_fk(self):
        self.rejects("INSERT INTO participants VALUES(999,999,'mentor')")

    def test_email_case_unique(self):
        self.rejects("INSERT INTO users(display_name,email) VALUES('Duplicate','A@EXAMPLE.TEST')")

    def test_roster_frozen(self):
        self.rejects("DELETE FROM participants WHERE user_id=30")
        self.rejects("INSERT INTO participants VALUES(1,1,'mentor')")
        self.rejects("UPDATE participants SET role='mentor' WHERE user_id=40")

    def test_freeze_counts(self):
        self.db.execute("INSERT INTO matching_rounds(id,name) VALUES(2,'Too few mentors')")
        self.db.execute("INSERT INTO participants VALUES(2,40,'mentee')")
        self.rejects("UPDATE matching_rounds SET state='frozen' WHERE id=2")

    def test_inactive_user_cannot_join(self):
        self.db.execute("INSERT INTO matching_rounds(id,name) VALUES(2,'New round')")
        self.db.execute("UPDATE users SET active=0 WHERE id=30")
        self.rejects("INSERT INTO participants VALUES(2,30,'mentor')")

    def test_promotion_preserves_history(self):
        self.db.execute("INSERT INTO matching_rounds(id,name) VALUES(2,'Next round')")
        self.db.execute("INSERT INTO participants VALUES(2,40,'mentor')")
        self.assertEqual(self.db.execute("SELECT role FROM participants WHERE round_id=1 AND user_id=40").fetchone()[0], "mentee")

    def test_change_reason_required(self):
        self.rejects("INSERT INTO preference_versions(round_id,owner_id,revision) VALUES(1,10,2)")
        self.rejects("INSERT INTO preference_versions(round_id,owner_id,revision,change_reason) VALUES(1,10,2,'   ')")

    def test_revision_sequential(self):
        self.rejects("INSERT INTO preference_versions(round_id,owner_id,revision,change_reason) VALUES(1,10,4,'why')")

    def test_complete_submission_required(self):
        v = self.draft()
        self.db.execute("INSERT INTO preference_entries VALUES(?,40,1)", (v,))
        self.rejects("UPDATE preference_versions SET state='submitted' WHERE id=?", (v,))

    def test_unique_ranks_and_targets(self):
        v = self.draft()
        self.db.execute("INSERT INTO preference_entries VALUES(?,40,1)", (v,))
        self.rejects("INSERT INTO preference_entries VALUES(?,50,1)", (v,))
        self.rejects("INSERT INTO preference_entries VALUES(?,40,2)", (v,))

    def test_rank_range_and_type(self):
        v = self.draft()
        self.rejects("INSERT INTO preference_entries VALUES(?,40,0)", (v,))
        self.rejects("INSERT INTO preference_entries VALUES(?,40,1.5)", (v,))
        self.db.executemany("INSERT INTO preference_entries VALUES(?,?,?)", [(v,40,1),(v,50,3)])
        self.rejects("UPDATE preference_versions SET state='submitted' WHERE id=?", (v,))

    def test_opposite_side_only(self):
        v = self.draft()
        self.rejects("INSERT INTO preference_entries VALUES(?,20,1)", (v,))
        self.rejects("INSERT INTO preference_entries VALUES(?,999,1)", (v,))

    def test_submitted_history_immutable(self):
        v = self.db.execute("SELECT id FROM preference_versions WHERE owner_id=10").fetchone()[0]
        self.rejects("DELETE FROM preference_entries WHERE version_id=?", (v,))
        self.rejects("UPDATE preference_entries SET rank=9 WHERE version_id=?", (v,))
        self.rejects("UPDATE preference_versions SET change_reason='rewrite' WHERE id=?", (v,))
        self.rejects("DELETE FROM preference_versions WHERE id=?", (v,))

    def test_stale_draft_cannot_submit(self):
        v = self.draft()
        self.db.executemany("INSERT INTO preference_entries VALUES(?,?,?)", [(v,40,1),(v,50,2)])
        self.db.execute("INSERT INTO preference_versions(round_id,owner_id,revision,change_reason) VALUES(1,10,3,'newer')")
        self.rejects("UPDATE preference_versions SET state='submitted' WHERE id=?", (v,))

    def test_new_preferences_preserve_run_and_mark_outdated(self):
        old = self.db.execute("SELECT version_id FROM run_preferences WHERE run_id=1 AND user_id=10").fetchone()[0]
        submit_preferences(self.db, 10, [50,40], 2, "Changed after introduction")
        self.assertEqual(self.db.execute("SELECT inputs_are_current FROM run_freshness WHERE run_id=1").fetchone()[0], 0)
        self.assertEqual(self.db.execute("SELECT version_id FROM run_preferences WHERE run_id=1 AND user_id=10").fetchone()[0], old)
        self.assertEqual(self.db.execute("SELECT state FROM matching_runs WHERE id=1").fetchone()[0], "verified")

    def test_run_requires_admin(self):
        self.rejects("INSERT INTO matching_runs(round_id,requested_by) VALUES(1,10)")
        self.db.execute("UPDATE users SET active=0 WHERE id=1")
        self.rejects("INSERT INTO matching_runs(round_id,requested_by) VALUES(1,1)")

    def test_snapshot_owner_fk(self):
        self.db.execute("INSERT INTO matching_runs(id,round_id,requested_by) VALUES(2,1,1)")
        v = self.db.execute("SELECT id FROM preference_versions WHERE owner_id=10").fetchone()[0]
        self.rejects("INSERT INTO run_preferences VALUES(2,1,20,?)", (v,))

    def test_snapshot_requires_submitted(self):
        v = self.draft()
        self.db.execute("INSERT INTO matching_runs(id,round_id,requested_by) VALUES(2,1,1)")
        self.rejects("INSERT INTO run_preferences VALUES(2,1,10,?)", (v,))

    def test_run_needs_all_input_versions(self):
        self.db.execute("INSERT INTO matching_runs(id,round_id,requested_by) VALUES(2,1,1)")
        self.db.execute("INSERT INTO verification_results VALUES(2,1,'Stable','fixture',NULL,NULL,CURRENT_TIMESTAMP)")
        self.rejects("UPDATE matching_runs SET state='verified' WHERE id=2")

    def test_assignment_roles_and_uniqueness(self):
        self.new_run()
        self.rejects("INSERT INTO assignments(run_id,round_id,mentor_id,mentee_id) VALUES(2,1,40,10)")
        self.db.execute("INSERT INTO assignments(run_id,round_id,mentor_id,mentee_id) VALUES(2,1,10,40)")
        self.rejects("INSERT INTO assignments(run_id,round_id,mentor_id,mentee_id) VALUES(2,1,10,50)")
        self.rejects("INSERT INTO assignments(run_id,round_id,mentor_id,mentee_id) VALUES(2,1,20,40)")

    def test_run_needs_verification(self):
        self.new_run()
        self.rejects("UPDATE matching_runs SET state='verified' WHERE id=2")

    def test_verified_run_requires_full_matching(self):
        self.new_run()
        self.db.execute("INSERT INTO verification_results VALUES(2,1,'Stable','fixture',NULL,NULL,CURRENT_TIMESTAMP)")
        self.rejects("UPDATE matching_runs SET state='verified' WHERE id=2")

    def test_rejected_run_records_unstable_pair(self):
        self.new_run()
        self.db.execute("INSERT INTO verification_results VALUES(2,1,'Unstable','fixture',10,50,CURRENT_TIMESTAMP)")
        self.rejects("UPDATE matching_runs SET state='verified' WHERE id=2")
        self.db.execute("UPDATE matching_runs SET state='rejected' WHERE id=2")

    def test_blocking_pair_shape(self):
        self.new_run()
        self.rejects("INSERT INTO verification_results VALUES(2,1,'Unstable','fixture',NULL,NULL,CURRENT_TIMESTAMP)")
        self.rejects("INSERT INTO verification_results VALUES(2,1,'Stable','fixture',10,50,CURRENT_TIMESTAMP)")
        self.rejects("INSERT INTO verification_results VALUES(2,1,'Unstable','fixture',40,10,CURRENT_TIMESTAMP)")

    def test_finalized_run_immutable(self):
        self.rejects("UPDATE matching_runs SET proposal_count=99 WHERE id=1")
        self.rejects("DELETE FROM matching_runs WHERE id=1")
        self.rejects("DELETE FROM assignments WHERE run_id=1")
        self.rejects("UPDATE assignments SET mentor_id=30 WHERE run_id=1")
        self.rejects("DELETE FROM run_preferences WHERE run_id=1")
        self.rejects("UPDATE verification_results SET status='InvalidAssignment' WHERE run_id=1")
        self.rejects("INSERT INTO assignments(run_id,round_id,mentor_id,mentee_id) VALUES(1,1,30,40)")

    def test_ratings_only_assigned_partners(self):
        a = self.db.execute("SELECT id FROM assignments WHERE mentor_id=10").fetchone()[0]
        self.db.execute("INSERT INTO ratings(assignment_id,author_id,target_id,score) VALUES(?,10,40,5)", (a,))
        self.rejects("INSERT INTO ratings(assignment_id,author_id,target_id,score) VALUES(?,10,40,4)", (a,))
        self.rejects("INSERT INTO ratings(assignment_id,author_id,target_id,score) VALUES(?,30,40,4)", (a,))
        self.rejects("INSERT INTO ratings(assignment_id,author_id,target_id,score) VALUES(?,40,10,6)", (a,))
        self.db.execute("INSERT INTO ratings(assignment_id,author_id,target_id,score) VALUES(?,40,10,3)", (a,))
        self.rejects("UPDATE ratings SET score=1 WHERE assignment_id=?", (a,))

    def test_replace_cannot_erase_history(self):
        v = self.db.execute("SELECT id FROM preference_versions WHERE owner_id=10").fetchone()[0]
        self.rejects("INSERT OR REPLACE INTO preference_entries VALUES(?,40,1)", (v,))
        self.rejects("INSERT OR REPLACE INTO assignments(id,run_id,round_id,mentor_id,mentee_id) "
                     "SELECT id,run_id,round_id,mentor_id,mentee_id FROM assignments WHERE mentor_id=10")
        a = self.db.execute("SELECT id FROM assignments WHERE mentor_id=10").fetchone()[0]
        self.db.execute("INSERT INTO ratings(assignment_id,author_id,target_id,score) VALUES(?,10,40,5)", (a,))
        self.rejects("INSERT OR REPLACE INTO ratings(assignment_id,author_id,target_id,score) VALUES(?,10,40,1)", (a,))

    def test_close_round(self):
        self.db.execute("UPDATE matching_rounds SET state='closed' WHERE id=1")
        self.rejects("INSERT INTO matching_runs(round_id,requested_by) VALUES(1,1)")
        self.rejects("INSERT INTO preference_versions(round_id,owner_id,revision,change_reason) VALUES(1,10,2,'why')")
        self.rejects("UPDATE matching_rounds SET state='frozen' WHERE id=1")

    def test_cannot_close_building_run(self):
        self.new_run()
        self.rejects("UPDATE matching_rounds SET state='closed' WHERE id=1")

    def test_atomic_rollback(self):
        before = self.db.execute("SELECT count(*) FROM preference_versions").fetchone()[0]
        try:
            with self.db:
                submit_preferences(self.db, 10, [40], 2, "Incomplete attempt")
        except sqlite3.IntegrityError:
            pass
        self.assertEqual(self.db.execute("SELECT count(*) FROM preference_versions").fetchone()[0], before)


if __name__ == "__main__":
    unittest.main(verbosity=2)
