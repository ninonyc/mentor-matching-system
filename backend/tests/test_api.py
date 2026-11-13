"""Local HTTP integration tests. Starts/stops its own C++ server and temporary DB."""
import hashlib
import json
import os
import runpy
from pathlib import Path
import socket
import sqlite3
import subprocess
import tempfile
import time
import unittest
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
PASSWORD = "Fictional-class-test-2026!"  # Disposable test accounts only.


class ApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.database = str(Path(cls.temp.name) / "integration.db")
        with socket.socket() as s:
            s.bind(("127.0.0.1", 0))
            cls.port = s.getsockname()[1]
        cls.base = f"http://127.0.0.1:{cls.port}"
        binary = os.environ.get("MENTOR_API_BINARY", str(ROOT / "build/mentor_api"))
        env = dict(os.environ, MENTOR_ALLOWED_ORIGIN="http://localhost:5173")
        cls.log = open(Path(cls.temp.name) / "server.log", "w+")
        cls.server = subprocess.Popen(
            [binary, "--database", cls.database, "--project-root", str(ROOT), "--init",
             "--admin-email", "admin@example.test", "--port", str(cls.port)],
            stdin=subprocess.PIPE, stdout=cls.log, stderr=cls.log, text=True, env=env,
        )
        cls.server.stdin.write(PASSWORD + "\n" + PASSWORD + "\n")
        cls.server.stdin.close()
        try:
            deadline = time.monotonic() + 30
            while True:
                if cls.server.poll() is not None:
                    cls.log.seek(0)
                    raise RuntimeError("Server startup failed: " + cls.log.read())
                try:
                    status, _ = cls.request("GET", "/api/health")
                    if status == 200:
                        break
                except OSError:
                    pass
                if time.monotonic() > deadline:
                    raise RuntimeError("Server startup timed out")
                time.sleep(0.05)
            cls.admin = cls.login("admin@example.test")
            cls.users, cls.tokens = {}, {}
            for name in ["m0", "m1", "m2", "e0", "e1", "outsider"]:
                status, user = cls.request("POST", "/api/users", {
                    "name": name, "email": name + "@example.test", "password": PASSWORD,
                }, cls.admin)
                if status != 201:
                    raise RuntimeError("Fixture user creation failed")
                cls.users[name] = user["id"]
                cls.tokens[name] = cls.login(name + "@example.test")
        except Exception:
            cls.server.terminate()
            cls.server.wait(timeout=10)
            cls.log.close()
            cls.temp.cleanup()
            raise

    @classmethod
    def tearDownClass(cls):
        cls.server.terminate()
        cls.server.wait(timeout=10)
        cls.log.close()
        cls.temp.cleanup()

    @classmethod
    def request(cls, method, path, body=None, token=None, headers=None, raw=None):
        h = dict(headers or {})
        if token:
            h["Authorization"] = "Bearer " + token
        data = None
        if body is not None:
            data = json.dumps(body).encode()
            h["Content-Type"] = "application/json"
        if raw is not None:
            data = raw
        request = urllib.request.Request(cls.base + path, data=data, headers=h, method=method)
        try:
            with urllib.request.urlopen(request, timeout=20) as response:
                payload = response.read()
                return response.status, json.loads(payload) if payload else {}
        except urllib.error.HTTPError as error:
            try:
                payload = error.read()
            finally:
                error.close()
            try:
                output = json.loads(payload)
            except (ValueError, UnicodeError):
                output = {}
            return error.code, output

    @classmethod
    def login(cls, email, password=PASSWORD):
        status, data = cls.request("POST", "/api/auth/login", {"email": email, "password": password})
        if status != 200:
            raise RuntimeError("Test login failed")
        return data["token"]

    def call(self, method, path, body=None, token=None, expected=200, **kwargs):
        status, result = self.request(method, path, body, token, **kwargs)
        self.assertEqual(status, expected, result.get("error", "Unexpected status"))
        return result

    def round(self, submit=True):
        r = self.call("POST", "/api/rounds", {"name": "Integration round"}, self.admin, 201)["id"]
        for name in ["m0", "m1", "m2", "e0", "e1"]:
            self.call("POST", f"/api/rounds/{r}/participants", {
                "user_id": self.users[name], "role": "mentor" if name.startswith("m") else "mentee",
            }, self.admin, 201)
        self.call("POST", f"/api/rounds/{r}/freeze", token=self.admin)
        if submit:
            self.preferences(r)
        return r

    def preferences(self, r):
        prefs = {"m0": ["e0", "e1"], "m1": ["e1", "e0"], "m2": ["e0", "e1"],
                 "e0": ["m0", "m2", "m1"], "e1": ["m1", "m2", "m0"]}
        for owner, targets in prefs.items():
            self.call("PUT", f"/api/rounds/{r}/preferences",
                      {"targets": [self.users[x] for x in targets]}, self.tokens[owner], 201)

    def matched(self):
        r = self.round()
        run = self.call("POST", f"/api/rounds/{r}/matches", token=self.admin, expected=201)
        return r, run

    def test_01_health(self):
        self.assertEqual(self.call("GET", "/api/health")["schema_version"], 2)

    def test_02_auth_required(self):
        self.call("GET", "/api/me", expected=401)
        self.call("GET", "/api/me", token="0" * 64, expected=401)

    def test_03_wrong_password(self):
        self.call("POST", "/api/auth/login", {"email": "admin@example.test", "password": "incorrect"}, expected=401)

    def test_04_json_validation(self):
        self.call("POST", "/api/auth/login", headers={"Content-Type": "application/json"}, raw=b"{", expected=400)
        self.call("POST", "/api/auth/login", body=[], expected=400)
        self.call("POST", "/api/auth/login", raw=b"email=test", expected=415)

    def test_04b_json_depth(self):
        nested = b'[' * 40 + b'0' + b']' * 40
        self.call("POST", "/api/auth/login", raw=nested, headers={"Content-Type": "application/json"}, expected=400)

    def test_05_origin(self):
        self.call("GET", "/api/me", token=self.admin, headers={"Origin": "https://untrusted.example"}, expected=403)
        self.call("OPTIONS", "/api/me", headers={"Origin": "http://localhost:5173"}, expected=204)

    def test_06_admin_user_creation(self):
        self.call("POST", "/api/users", {"name": "x", "email": "x@example.test", "password": PASSWORD}, self.tokens["m0"], 403)
        self.call("GET", "/api/users", token=self.tokens["m0"], expected=403)
        self.call("POST", "/api/users", {"name": "duplicate", "email": "M0@EXAMPLE.TEST", "password": PASSWORD}, self.admin, 409)
        self.call("POST", "/api/users", {"name": "bad", "email": "bad@example.test", "password": "short"}, self.admin, 400)

    def test_07_input_types(self):
        self.call("PATCH", f"/api/users/{self.users['m0']}", {"active": "false"}, self.admin, 400)
        r = self.round(False)
        self.call("PUT", f"/api/rounds/{r}/preferences", {"targets": [True]}, self.tokens["m0"], 400)
        self.call("PUT", f"/api/rounds/{r}/preferences", {"targets": [2**64-1]}, self.tokens["m0"], 400)

    def test_08_readiness_and_missing_preferences(self):
        r = self.round(False)
        ready = self.call("GET", f"/api/rounds/{r}/readiness", token=self.admin)
        self.assertFalse(ready["ready"])
        self.assertEqual(len(ready["missing_preferences"]), 5)
        self.call("POST", f"/api/rounds/{r}/matches", token=self.admin, expected=409)
        self.assertEqual(self.call("GET", f"/api/rounds/{r}/matches", token=self.admin)["runs"], [])

    def test_09_preference_rollback(self):
        r = self.round(False)
        self.call("PUT", f"/api/rounds/{r}/preferences", {"targets": [self.users["e0"]]}, self.tokens["m0"], 409)
        with sqlite3.connect(self.database) as db:
            self.assertEqual(db.execute("SELECT count(*) FROM preference_versions WHERE round_id=?", (r,)).fetchone()[0], 0)

    def test_10_duplicate_preference(self):
        r = self.round(False)
        self.call("PUT", f"/api/rounds/{r}/preferences", {"targets": [self.users["e0"]] * 2}, self.tokens["m0"], 400)

    def test_11_self_owned_preferences(self):
        r = self.round(False)
        targets = [self.users[x] for x in ["m0", "m1", "m2"]]
        # A caller-supplied owner does not override the authenticated identity.
        self.call("PUT", f"/api/rounds/{r}/preferences", {"owner_id": self.users["m0"], "targets": targets}, self.tokens["e0"], 201)
        own = self.call("GET", f"/api/rounds/{r}/preferences", token=self.tokens["e0"])
        self.assertEqual(own["targets"], targets)
        self.call("GET", f"/api/rounds/{r}/preferences", token=self.tokens["m0"], expected=404)

    def test_12_matching_calls_real_core(self):
        r, run = self.matched()
        self.assertEqual(run["state"], "verified")
        self.assertEqual(run["verification"]["status"], "Stable")
        self.assertEqual(run["proposal_count"], 4)
        self.assertTrue(run["inputs_are_current"])
        self.assertEqual({(x["mentor_id"], x["mentee_id"]) for x in run["assignments"]},
                         {(self.users["m0"], self.users["e0"]), (self.users["m1"], self.users["e1"])})
        self.assertTrue(self.call("GET", f"/api/rounds/{r}/readiness", token=self.admin)["ready"])
        with sqlite3.connect(self.database) as db:
            self.assertEqual(db.execute("SELECT count(*) FROM run_preferences WHERE run_id=?", (run["id"],)).fetchone()[0], 5)

    def test_13_matching_admin_only(self):
        r = self.round()
        self.call("POST", f"/api/rounds/{r}/matches", token=self.tokens["m0"], expected=403)
        self.call("POST", f"/api/rounds/{r}/rechecks", {"run_id": 1}, self.tokens["e0"], 403)

    def test_14_assignment_privacy(self):
        r, run = self.matched()
        own = self.call("GET", f"/api/matches/{run['id']}", token=self.tokens["e0"])
        self.assertEqual(len(own["assignments"]), 1)
        self.assertEqual(own["assignments"][0]["mentee_id"], self.users["e0"])
        self.assertNotIn("verification", own)
        free = self.call("GET", f"/api/rounds/{r}/my-assignment", token=self.tokens["m2"])
        self.assertEqual(free["assignments"], [])
        self.call("GET", f"/api/matches/{run['id']}", token=self.tokens["outsider"], expected=403)
        self.call("GET", f"/api/rounds/{r}/matches", token=self.tokens["e0"], expected=403)

    def test_15_change_reason_and_snapshot(self):
        r, run = self.matched()
        targets = [self.users[x] for x in ["e1", "e0"]]
        self.call("PUT", f"/api/rounds/{r}/preferences", {"targets": targets}, self.tokens["m0"], 409)
        self.call("PUT", f"/api/rounds/{r}/preferences", {"targets": targets, "reason": "Changed after meeting"}, self.tokens["m0"], 201)
        historical = self.call("GET", f"/api/matches/{run['id']}", token=self.admin)
        self.assertEqual(historical["state"], "verified")
        self.assertFalse(historical["inputs_are_current"])
        self.assertEqual(historical["assignments"], run["assignments"])

    def test_16_recheck_unstable(self):
        r, run = self.matched()
        for owner, order in [("m0", ["e1", "e0"]), ("e1", ["m0", "m1", "m2"])]:
            self.call("PUT", f"/api/rounds/{r}/preferences", {"targets": [self.users[x] for x in order], "reason": "Changed after introduction"}, self.tokens[owner], 201)
        checked = self.call("POST", f"/api/rounds/{r}/rechecks", {"run_id": run["id"]}, self.admin, 201)
        self.assertEqual(checked["state"], "rejected")
        self.assertEqual(checked["kind"], "recheck")
        self.assertEqual(checked["source_run_id"], run["id"])
        self.assertEqual(checked["proposal_count"], 0)
        self.assertEqual(checked["verification"]["blocking_pair"], {"mentor_id": self.users["m0"], "mentee_id": self.users["e1"]})
        self.call("GET", f"/api/matches/{checked['id']}", token=self.tokens["e0"], expected=403)
        rematched = self.call("POST", f"/api/rounds/{r}/matches", token=self.admin, expected=201)
        self.assertEqual(rematched["state"], "verified")

    def test_17_change_can_remain_stable(self):
        r, run = self.matched()
        self.call("PUT", f"/api/rounds/{r}/preferences", {"targets": [self.users[x] for x in ["m0", "m1", "m2"]], "reason": "Changed lower preferences"}, self.tokens["e0"], 201)
        checked = self.call("POST", f"/api/rounds/{r}/rechecks", {"run_id": run["id"]}, self.admin, 201)
        self.assertEqual(checked["state"], "verified")
        self.assertTrue(checked["inputs_are_current"])

    def test_18_ratings_privacy_and_ownership(self):
        _, run = self.matched()
        assignment = next(x for x in run["assignments"] if x["mentor_id"] == self.users["m0"])
        payload = {"assignment_id": assignment["id"], "score": 5, "comment": "Useful mentoring"}
        self.call("POST", "/api/ratings", payload, self.tokens["m2"], 403)
        self.call("POST", "/api/ratings", payload, self.tokens["e0"], 201)
        self.call("POST", "/api/ratings", payload, self.tokens["e0"], 409)
        self.call("GET", "/api/ratings", token=self.tokens["e0"], expected=403)
        ratings = self.call("GET", "/api/ratings", token=self.admin)["ratings"]
        entry = next(x for x in ratings if x["assignment_id"] == assignment["id"])
        self.assertEqual(entry["author_id"], self.users["e0"])
        self.assertEqual(entry["target_id"], self.users["m0"])

    def test_19_closed_round(self):
        r = self.round()
        self.call("POST", f"/api/rounds/{r}/close", token=self.admin)
        self.call("POST", f"/api/rounds/{r}/matches", token=self.admin, expected=409)
        self.call("PUT", f"/api/rounds/{r}/preferences", {"targets": [self.users["e0"], self.users["e1"]], "reason": "late"}, self.tokens["m0"], 409)

    def test_20_frozen_roster(self):
        r = self.round(False)
        self.call("DELETE", f"/api/rounds/{r}/participants/{self.users['m2']}", token=self.admin, expected=409)
        self.call("POST", f"/api/rounds/{r}/participants", {"user_id": self.users["outsider"], "role": "mentor"}, self.admin, 409)

    def test_21_collecting_roster_remove(self):
        r = self.call("POST", "/api/rounds", {"name": "Collecting"}, self.admin, 201)["id"]
        self.call("POST", f"/api/rounds/{r}/participants", {"user_id": self.users["m0"], "role": "mentor"}, self.admin, 201)
        self.call("DELETE", f"/api/rounds/{r}/participants/{self.users['m0']}", token=self.admin)
        self.assertEqual(self.call("GET", f"/api/rounds/{r}/participants", token=self.admin)["participants"], [])

    def test_22_not_found_and_injection(self):
        self.call("GET", "/api/matches/999999", token=self.admin, expected=404)
        self.call("GET", "/api/rounds/1%20OR%201/readiness", token=self.admin, expected=400)
        self.call("POST", "/api/auth/login", {"email": "' OR 1=1 --", "password": PASSWORD}, expected=401)

    def test_23_expired_session(self):
        token = self.login("m2@example.test")
        with sqlite3.connect(self.database) as db:
            db.execute("UPDATE sessions SET expires_at=0 WHERE token_hash=?", (hashlib.sha256(token.encode()).hexdigest(),))
        self.call("GET", "/api/me", token=token, expected=401)

    def test_24_logout(self):
        token = self.login("m2@example.test")
        self.call("POST", "/api/auth/logout", token=token)
        self.call("GET", "/api/me", token=token, expected=401)

    def test_25_password_change(self):
        user = self.call("POST", "/api/users", {"name": "password test", "email": "password@example.test", "password": PASSWORD}, self.admin, 201)
        token = self.login("password@example.test")
        self.call("POST", "/api/me/password", {"current_password": "incorrect", "new_password": PASSWORD + "new"}, token, 401)
        self.call("POST", "/api/me/password", {"current_password": PASSWORD, "new_password": PASSWORD + "new"}, token)
        self.call("GET", "/api/me", token=token, expected=401)
        self.call("POST", "/api/auth/login", {"email": "password@example.test", "password": PASSWORD}, expected=401)
        new_token = self.login("password@example.test", PASSWORD + "new")
        self.assertEqual(self.call("GET", "/api/me", token=new_token)["id"], user["id"])

    def test_26_deactivation(self):
        user = self.call("POST", "/api/users", {"name": "deactivation test", "email": "disabled@example.test", "password": PASSWORD}, self.admin, 201)
        token = self.login("disabled@example.test")
        self.call("PATCH", f"/api/users/{user['id']}", {"active": False}, self.admin)
        self.call("GET", "/api/me", token=token, expected=401)
        self.call("POST", "/api/auth/login", {"email": "disabled@example.test", "password": PASSWORD}, expected=401)
        self.call("PATCH", "/api/users/1", {"active": False}, self.admin, 409)

    def test_27_secrets_not_stored_plaintext(self):
        with sqlite3.connect(self.database) as db:
            salts = [x[0] for x in db.execute("SELECT salt FROM credentials")]
            self.assertEqual(len(salts), len(set(salts)))
            self.assertTrue(all(len(x) == 32 for x in salts))
            self.assertNotIn(self.admin, [x[0] for x in db.execute("SELECT token_hash FROM sessions")])
            self.assertTrue(all(x[0] != PASSWORD for x in db.execute("SELECT password_hash FROM credentials")))
            self.assertEqual(db.execute("PRAGMA foreign_key_check").fetchall(), [])

    def test_28_oversize_body(self):
        result = self.call("POST", "/api/auth/login", raw=b"x" * 70000, headers={"Content-Type": "application/json"}, expected=413)
        self.assertIn("error", result)

    def test_29_empty_round(self):
        r = self.call("POST", "/api/rounds", {"name": "Empty round"}, self.admin, 201)["id"]
        self.call("POST", f"/api/rounds/{r}/freeze", token=self.admin)
        run = self.call("POST", f"/api/rounds/{r}/matches", token=self.admin, expected=201)
        self.assertEqual(run["state"], "verified")
        self.assertEqual(run["assignments"], [])

    def test_30_bootstrap_cannot_reset_accounts(self):
        binary = os.environ.get("MENTOR_API_BINARY", str(ROOT / "build/mentor_api"))
        result = subprocess.run([binary, "--database", self.database, "--admin-email", "admin@example.test"],
                                input=PASSWORD + "\n" + PASSWORD + "\n", capture_output=True, text=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Bootstrap is disabled", result.stderr)

    def test_31_legacy_migration_preserves_history(self):
        fixture = runpy.run_path(str(ROOT / "database/demo.py"))
        legacy = str(Path(self.temp.name) / "legacy.db")
        db = fixture["connect"](legacy)
        db.executescript(fixture["SCHEMA"].read_text())
        fixture["seed_demo"](db)
        before = db.execute("SELECT mentor_id,mentee_id FROM assignments ORDER BY id").fetchall()
        db.close()
        binary = os.environ.get("MENTOR_API_BINARY", str(ROOT / "build/mentor_api"))
        result = subprocess.run([binary, "--init", "--migrate-only", "--database", legacy,
                                 "--project-root", str(ROOT)], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        with sqlite3.connect(legacy) as db:
            self.assertEqual(db.execute("PRAGMA user_version").fetchone()[0], 2)
            self.assertEqual(db.execute("SELECT mentor_id,mentee_id FROM assignments ORDER BY id").fetchall(), before)
            self.assertEqual(db.execute("SELECT count(*) FROM credentials").fetchone()[0], 0)
            self.assertEqual(db.execute("PRAGMA foreign_key_check").fetchall(), [])

    def test_32_admin_preference_overview(self):
        r = self.round(False)
        pending = self.call("GET", f"/api/rounds/{r}/preferences-overview", token=self.admin)
        self.assertEqual(pending["missing"], 5)
        self.assertTrue(all(x["targets"] is None for x in pending["participants"]))
        self.preferences(r)
        ready = self.call("GET", f"/api/rounds/{r}/preferences-overview", token=self.admin)
        self.assertTrue(ready["ready"])
        self.assertEqual(len(ready["participants"]), 5)
        self.call("GET", f"/api/rounds/{r}/preferences-overview", token=self.tokens["m0"], expected=403)

    def test_99_login_rate_limit(self):
        for _ in range(21):
            status, _ = self.request("POST", "/api/auth/login", {"email": "nobody@example.test", "password": PASSWORD})
            if status == 429:
                return
            self.assertEqual(status, 401)
        self.fail("Expected login rate limiting")


if __name__ == "__main__":
    unittest.main(verbosity=2)
