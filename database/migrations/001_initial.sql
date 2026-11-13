-- Migration 001. Apply once to a NEW database. Never reapply over existing data.
-- Enable this pragma on EVERY connection, before beginning transactions.
PRAGMA foreign_keys = ON;
PRAGMA recursive_triggers = ON;
BEGIN IMMEDIATE;

CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    display_name TEXT NOT NULL CHECK(length(trim(display_name)) > 0),
    email TEXT NOT NULL COLLATE NOCASE UNIQUE CHECK(length(trim(email)) > 0),
    is_admin INTEGER NOT NULL DEFAULT 0 CHECK(is_admin IN (0,1)),
    active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1)),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE matching_rounds (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL CHECK(length(trim(name)) > 0),
    state TEXT NOT NULL DEFAULT 'collecting' CHECK(state IN ('collecting','frozen','closed')),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- A person's role belongs to a round; promotion happens in a subsequent round.
CREATE TABLE participants (
    round_id INTEGER NOT NULL REFERENCES matching_rounds(id),
    user_id INTEGER NOT NULL REFERENCES users(id),
    role TEXT NOT NULL CHECK(role IN ('mentor','mentee')),
    PRIMARY KEY(round_id,user_id)
);

CREATE TABLE preference_versions (
    id INTEGER PRIMARY KEY,
    round_id INTEGER NOT NULL,
    owner_id INTEGER NOT NULL,
    revision INTEGER NOT NULL CHECK(typeof(revision) = 'integer' AND revision > 0),
    change_reason TEXT,
    state TEXT NOT NULL DEFAULT 'draft' CHECK(state IN ('draft','submitted')),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(round_id,owner_id,revision),
    UNIQUE(id,round_id,owner_id),
    FOREIGN KEY(round_id,owner_id) REFERENCES participants(round_id,user_id)
);

CREATE TABLE preference_entries (
    version_id INTEGER NOT NULL REFERENCES preference_versions(id),
    target_id INTEGER NOT NULL REFERENCES users(id),
    rank INTEGER NOT NULL CHECK(typeof(rank) = 'integer' AND rank > 0),
    PRIMARY KEY(version_id,target_id),
    UNIQUE(version_id,rank)
);

CREATE TABLE matching_runs (
    id INTEGER PRIMARY KEY,
    round_id INTEGER NOT NULL REFERENCES matching_rounds(id),
    requested_by INTEGER NOT NULL REFERENCES users(id),
    state TEXT NOT NULL DEFAULT 'building' CHECK(state IN ('building','verified','rejected')),
    proposal_count INTEGER NOT NULL DEFAULT 0 CHECK(typeof(proposal_count) = 'integer' AND proposal_count >= 0),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(id,round_id)
);

-- One exact submitted preference version per participant for each run.
CREATE TABLE run_preferences (
    run_id INTEGER NOT NULL,
    round_id INTEGER NOT NULL,
    user_id INTEGER NOT NULL,
    version_id INTEGER NOT NULL,
    PRIMARY KEY(run_id,user_id),
    FOREIGN KEY(run_id,round_id) REFERENCES matching_runs(id,round_id),
    FOREIGN KEY(version_id,round_id,user_id) REFERENCES preference_versions(id,round_id,owner_id)
);

CREATE TABLE assignments (
    id INTEGER PRIMARY KEY,
    run_id INTEGER NOT NULL,
    round_id INTEGER NOT NULL,
    mentor_id INTEGER NOT NULL,
    mentee_id INTEGER NOT NULL,
    UNIQUE(run_id,mentor_id),
    UNIQUE(run_id,mentee_id),
    FOREIGN KEY(run_id,round_id) REFERENCES matching_runs(id,round_id),
    FOREIGN KEY(round_id,mentor_id) REFERENCES participants(round_id,user_id),
    FOREIGN KEY(round_id,mentee_id) REFERENCES participants(round_id,user_id),
    CHECK(mentor_id != mentee_id)
);

CREATE TABLE verification_results (
    run_id INTEGER PRIMARY KEY,
    round_id INTEGER NOT NULL,
    status TEXT NOT NULL CHECK(status IN ('Stable','Unstable','InvalidAssignment','InvalidPreferences')),
    explanation TEXT NOT NULL CHECK(length(trim(explanation)) > 0),
    blocking_mentor_id INTEGER,
    blocking_mentee_id INTEGER,
    verified_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(run_id,round_id) REFERENCES matching_runs(id,round_id),
    FOREIGN KEY(round_id,blocking_mentor_id) REFERENCES participants(round_id,user_id),
    FOREIGN KEY(round_id,blocking_mentee_id) REFERENCES participants(round_id,user_id),
    CHECK((status = 'Unstable' AND blocking_mentor_id IS NOT NULL AND blocking_mentee_id IS NOT NULL)
       OR (status != 'Unstable' AND blocking_mentor_id IS NULL AND blocking_mentee_id IS NULL))
);

CREATE TABLE ratings (
    id INTEGER PRIMARY KEY,
    assignment_id INTEGER NOT NULL REFERENCES assignments(id),
    author_id INTEGER NOT NULL REFERENCES users(id),
    target_id INTEGER NOT NULL REFERENCES users(id),
    score INTEGER NOT NULL CHECK(typeof(score) = 'integer' AND score BETWEEN 1 AND 5),
    comment TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(assignment_id,author_id),
    CHECK(author_id != target_id)
);

CREATE INDEX preference_latest ON preference_versions(round_id,owner_id,state,revision DESC);
CREATE INDEX assignments_round ON assignments(round_id);

CREATE TRIGGER round_start BEFORE INSERT ON matching_rounds
WHEN NEW.state != 'collecting'
BEGIN SELECT RAISE(ABORT,'New rounds start collecting'); END;

CREATE TRIGGER round_transition BEFORE UPDATE ON matching_rounds
WHEN NEW.id != OLD.id OR NEW.name != OLD.name OR NEW.created_at != OLD.created_at
 OR NOT ((OLD.state = 'collecting' AND NEW.state = 'frozen')
      OR (OLD.state = 'frozen' AND NEW.state = 'closed'))
BEGIN SELECT RAISE(ABORT,'Only collecting -> frozen -> closed transitions are allowed'); END;

CREATE TRIGGER round_freeze BEFORE UPDATE OF state ON matching_rounds
WHEN NEW.state = 'frozen'
BEGIN
 SELECT CASE WHEN
  (SELECT count(*) FROM participants WHERE round_id=NEW.id AND role='mentor') <
  (SELECT count(*) FROM participants WHERE round_id=NEW.id AND role='mentee')
 THEN RAISE(ABORT,'Cannot freeze a round with fewer mentors than mentees') END;
END;

CREATE TRIGGER preference_create BEFORE INSERT ON preference_versions
BEGIN
 SELECT CASE WHEN NEW.state != 'draft' OR
  (SELECT state FROM matching_rounds WHERE id=NEW.round_id) != 'frozen'
 THEN RAISE(ABORT,'Create draft preferences in a frozen round') END;
 SELECT CASE WHEN NEW.revision != COALESCE((SELECT max(revision)+1 FROM preference_versions
  WHERE round_id=NEW.round_id AND owner_id=NEW.owner_id),1)
 THEN RAISE(ABORT,'Preference revision must be the next number') END;
 SELECT CASE WHEN EXISTS(SELECT 1 FROM preference_versions WHERE round_id=NEW.round_id
  AND owner_id=NEW.owner_id AND state='submitted') AND length(trim(COALESCE(NEW.change_reason,'')))=0
 THEN RAISE(ABORT,'Changing submitted preferences requires an explanation') END;
END;

CREATE TRIGGER preference_submit BEFORE UPDATE ON preference_versions
BEGIN
 SELECT CASE WHEN OLD.state != 'draft' OR NEW.state != 'submitted'
  OR NEW.id != OLD.id OR NEW.round_id != OLD.round_id OR NEW.owner_id != OLD.owner_id
  OR NEW.revision != OLD.revision OR NEW.change_reason IS NOT OLD.change_reason
  OR NEW.created_at != OLD.created_at
  OR (SELECT state FROM matching_rounds WHERE id=NEW.round_id) != 'frozen'
 THEN RAISE(ABORT,'Only draft-to-submitted transition is allowed') END;
 SELECT CASE WHEN NEW.revision != (SELECT max(revision) FROM preference_versions
  WHERE round_id=NEW.round_id AND owner_id=NEW.owner_id)
 THEN RAISE(ABORT,'Only the newest draft can be submitted') END;
 SELECT CASE WHEN
  (SELECT count(*) FROM preference_entries WHERE version_id=NEW.id) !=
  (SELECT count(*) FROM participants WHERE round_id=NEW.round_id AND role !=
    (SELECT role FROM participants WHERE round_id=NEW.round_id AND user_id=NEW.owner_id))
 OR EXISTS(SELECT 1 FROM preference_entries WHERE version_id=NEW.id AND rank >
  (SELECT count(*) FROM participants WHERE round_id=NEW.round_id AND role !=
    (SELECT role FROM participants WHERE round_id=NEW.round_id AND user_id=NEW.owner_id)))
 THEN RAISE(ABORT,'Submitted rankings must include everyone with ranks 1 through group size') END;
END;

CREATE TRIGGER entry_target BEFORE INSERT ON preference_entries
WHEN NOT EXISTS(SELECT 1 FROM preference_versions v
 JOIN participants owner ON owner.round_id=v.round_id AND owner.user_id=v.owner_id
 JOIN participants target ON target.round_id=v.round_id AND target.user_id=NEW.target_id
 WHERE v.id=NEW.version_id AND owner.role != target.role)
BEGIN SELECT RAISE(ABORT,'Preference target must be on the opposite side in the same round'); END;

CREATE TRIGGER run_create BEFORE INSERT ON matching_runs
BEGIN
 SELECT CASE WHEN NEW.state != 'building' OR
  (SELECT state FROM matching_rounds WHERE id=NEW.round_id) != 'frozen'
 THEN RAISE(ABORT,'Runs start building in frozen rounds') END;
 SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM users WHERE id=NEW.requested_by AND active=1 AND is_admin=1)
 THEN RAISE(ABORT,'Run requester must be an active administrator') END;
END;

CREATE TRIGGER snapshot_submitted BEFORE INSERT ON run_preferences
WHEN NOT EXISTS(SELECT 1 FROM preference_versions WHERE id=NEW.version_id AND state='submitted')
BEGIN SELECT RAISE(ABORT,'Run inputs must reference submitted preferences'); END;

CREATE TRIGGER assignment_roles BEFORE INSERT ON assignments
WHEN (SELECT role FROM participants WHERE round_id=NEW.round_id AND user_id=NEW.mentor_id) != 'mentor'
 OR (SELECT role FROM participants WHERE round_id=NEW.round_id AND user_id=NEW.mentee_id) != 'mentee'
BEGIN SELECT RAISE(ABORT,'Assignment endpoints have incorrect roles'); END;

CREATE TRIGGER witness_roles BEFORE INSERT ON verification_results
WHEN NEW.status='Unstable' AND (
 (SELECT role FROM participants WHERE round_id=NEW.round_id AND user_id=NEW.blocking_mentor_id) != 'mentor'
 OR (SELECT role FROM participants WHERE round_id=NEW.round_id AND user_id=NEW.blocking_mentee_id) != 'mentee')
BEGIN SELECT RAISE(ABORT,'Blocking-pair endpoints have incorrect roles'); END;

CREATE TRIGGER run_finalize BEFORE UPDATE ON matching_runs
BEGIN
 SELECT CASE WHEN OLD.state != 'building' OR NEW.state NOT IN ('verified','rejected')
  OR NEW.id != OLD.id OR NEW.round_id != OLD.round_id OR NEW.requested_by != OLD.requested_by
  OR NEW.created_at != OLD.created_at
 THEN RAISE(ABORT,'Only building runs can be finalized') END;
 SELECT CASE WHEN
  (SELECT count(*) FROM run_preferences WHERE run_id=NEW.id) !=
  (SELECT count(*) FROM participants WHERE round_id=NEW.round_id)
 THEN RAISE(ABORT,'Every participant needs an exact input version in the run') END;
 SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM verification_results WHERE run_id=NEW.id
   AND ((NEW.state='verified' AND status='Stable') OR (NEW.state='rejected' AND status!='Stable')))
 THEN RAISE(ABORT,'Run state must agree with verification result') END;
 SELECT CASE WHEN NEW.state='verified' AND
  (SELECT count(*) FROM assignments WHERE run_id=NEW.id) !=
  (SELECT count(*) FROM participants WHERE round_id=NEW.round_id AND role='mentee')
 THEN RAISE(ABORT,'Verified runs must assign every mentee') END;
END;

CREATE TRIGGER rating_pair BEFORE INSERT ON ratings
WHEN NOT EXISTS(SELECT 1 FROM assignments a JOIN matching_runs r ON r.id=a.run_id
 WHERE a.id=NEW.assignment_id AND r.state='verified' AND
 ((NEW.author_id=a.mentor_id AND NEW.target_id=a.mentee_id)
 OR (NEW.author_id=a.mentee_id AND NEW.target_id=a.mentor_id)))
BEGIN SELECT RAISE(ABORT,'Only assigned partners may rate each other in verified runs'); END;

CREATE TRIGGER participant_insert BEFORE INSERT ON participants
WHEN (SELECT state FROM matching_rounds WHERE id=NEW.round_id) != 'collecting'
BEGIN SELECT RAISE(ABORT,'Roster can change only while collecting'); END;

CREATE TRIGGER participant_delete BEFORE DELETE ON participants
WHEN (SELECT state FROM matching_rounds WHERE id=OLD.round_id) != 'collecting'
BEGIN SELECT RAISE(ABORT,'Roster can change only while collecting'); END;

CREATE TRIGGER participant_active BEFORE INSERT ON participants
WHEN NOT EXISTS(SELECT 1 FROM users WHERE id=NEW.user_id AND active=1)
BEGIN SELECT RAISE(ABORT,'Only active users can join a round'); END;

CREATE TRIGGER entry_insert BEFORE INSERT ON preference_entries
WHEN NOT EXISTS(SELECT 1 FROM preference_versions v JOIN matching_rounds r ON r.id=v.round_id
 WHERE v.id=NEW.version_id AND v.state='draft' AND r.state='frozen')
BEGIN SELECT RAISE(ABORT,'Only draft entries in frozen rounds can change'); END;

CREATE TRIGGER entry_delete BEFORE DELETE ON preference_entries
WHEN NOT EXISTS(SELECT 1 FROM preference_versions v JOIN matching_rounds r ON r.id=v.round_id
 WHERE v.id=OLD.version_id AND v.state='draft' AND r.state='frozen')
BEGIN SELECT RAISE(ABORT,'Only draft entries in frozen rounds can change'); END;

CREATE TRIGGER run_preferences_building BEFORE INSERT ON run_preferences
WHEN NOT EXISTS(SELECT 1 FROM matching_runs WHERE id=NEW.run_id AND state='building')
BEGIN SELECT RAISE(ABORT,'Run data can be inserted only while building'); END;

CREATE TRIGGER assignments_building BEFORE INSERT ON assignments
WHEN NOT EXISTS(SELECT 1 FROM matching_runs WHERE id=NEW.run_id AND state='building')
BEGIN SELECT RAISE(ABORT,'Run data can be inserted only while building'); END;

CREATE TRIGGER verification_results_building BEFORE INSERT ON verification_results
WHEN NOT EXISTS(SELECT 1 FROM matching_runs WHERE id=NEW.run_id AND state='building')
BEGIN SELECT RAISE(ABORT,'Run data can be inserted only while building'); END;

CREATE TRIGGER participants_no_update BEFORE UPDATE ON participants
BEGIN SELECT RAISE(ABORT,'Historical records cannot be rewritten or deleted'); END;

CREATE TRIGGER preference_versions_no_delete BEFORE DELETE ON preference_versions
BEGIN SELECT RAISE(ABORT,'Historical records cannot be rewritten or deleted'); END;

CREATE TRIGGER preference_entries_no_update BEFORE UPDATE ON preference_entries
BEGIN SELECT RAISE(ABORT,'Historical records cannot be rewritten or deleted'); END;

CREATE TRIGGER matching_rounds_no_delete BEFORE DELETE ON matching_rounds
BEGIN SELECT RAISE(ABORT,'Historical records cannot be rewritten or deleted'); END;

CREATE TRIGGER matching_runs_no_delete BEFORE DELETE ON matching_runs
BEGIN SELECT RAISE(ABORT,'Historical records cannot be rewritten or deleted'); END;

CREATE TRIGGER run_preferences_no_update BEFORE UPDATE ON run_preferences
BEGIN SELECT RAISE(ABORT,'Historical records cannot be rewritten or deleted'); END;

CREATE TRIGGER run_preferences_no_delete BEFORE DELETE ON run_preferences
BEGIN SELECT RAISE(ABORT,'Historical records cannot be rewritten or deleted'); END;

CREATE TRIGGER assignments_no_update BEFORE UPDATE ON assignments
BEGIN SELECT RAISE(ABORT,'Historical records cannot be rewritten or deleted'); END;

CREATE TRIGGER assignments_no_delete BEFORE DELETE ON assignments
BEGIN SELECT RAISE(ABORT,'Historical records cannot be rewritten or deleted'); END;

CREATE TRIGGER verification_results_no_update BEFORE UPDATE ON verification_results
BEGIN SELECT RAISE(ABORT,'Historical records cannot be rewritten or deleted'); END;

CREATE TRIGGER verification_results_no_delete BEFORE DELETE ON verification_results
BEGIN SELECT RAISE(ABORT,'Historical records cannot be rewritten or deleted'); END;

CREATE TRIGGER ratings_no_update BEFORE UPDATE ON ratings
BEGIN SELECT RAISE(ABORT,'Historical records cannot be rewritten or deleted'); END;

CREATE TRIGGER ratings_no_delete BEFORE DELETE ON ratings
BEGIN SELECT RAISE(ABORT,'Historical records cannot be rewritten or deleted'); END;

-- Historical verification remains valid for its stored inputs. This view
-- identifies whether those inputs are still the latest submitted rankings.
CREATE VIEW run_freshness AS
SELECT r.id AS run_id,
 CASE WHEN
  (SELECT count(*) FROM run_preferences rp WHERE rp.run_id=r.id) =
  (SELECT count(*) FROM participants p WHERE p.round_id=r.round_id)
  AND NOT EXISTS(SELECT 1 FROM run_preferences rp
   JOIN preference_versions old ON old.id=rp.version_id
   JOIN preference_versions newer ON newer.round_id=old.round_id
     AND newer.owner_id=old.owner_id AND newer.state='submitted'
     AND newer.revision > old.revision WHERE rp.run_id=r.id)
 THEN 1 ELSE 0 END AS inputs_are_current
FROM matching_runs r;

CREATE TRIGGER round_close BEFORE UPDATE OF state ON matching_rounds
WHEN NEW.state='closed' AND EXISTS(
 SELECT 1 FROM matching_runs WHERE round_id=NEW.id AND state='building')
BEGIN SELECT RAISE(ABORT,'Finalize building runs before closing a round'); END;

PRAGMA user_version = 1;
COMMIT;
