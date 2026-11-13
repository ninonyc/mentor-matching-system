-- Apply once after migration 001, with foreign_keys/recursive_triggers ON.
BEGIN IMMEDIATE;
CREATE TABLE credentials (
 user_id INTEGER PRIMARY KEY REFERENCES users(id),
 salt TEXT NOT NULL CHECK(length(salt)=32),
 password_hash TEXT NOT NULL CHECK(length(password_hash)=64)
);
CREATE TABLE sessions (
 token_hash TEXT PRIMARY KEY CHECK(length(token_hash)=64),
 user_id INTEGER NOT NULL REFERENCES users(id),
 expires_at INTEGER NOT NULL
);
CREATE INDEX sessions_user ON sessions(user_id);
ALTER TABLE matching_runs ADD COLUMN kind TEXT NOT NULL DEFAULT 'matching'
 CHECK(kind IN ('matching','recheck'));
ALTER TABLE matching_runs ADD COLUMN source_run_id INTEGER REFERENCES matching_runs(id);
CREATE TRIGGER run_source_insert BEFORE INSERT ON matching_runs
WHEN (NEW.kind='matching' AND NEW.source_run_id IS NOT NULL)
 OR (NEW.kind='recheck' AND (NEW.source_run_id IS NULL OR NOT EXISTS(
  SELECT 1 FROM matching_runs source WHERE source.id=NEW.source_run_id
   AND source.round_id=NEW.round_id AND source.state='verified')))
BEGIN SELECT RAISE(ABORT,'Rechecks must reference a verified source in the same round'); END;
CREATE TRIGGER run_source_immutable BEFORE UPDATE ON matching_runs
WHEN NEW.kind IS NOT OLD.kind OR NEW.source_run_id IS NOT OLD.source_run_id
BEGIN SELECT RAISE(ABORT,'Run kind and source are immutable'); END;
PRAGMA user_version=2;
COMMIT;
