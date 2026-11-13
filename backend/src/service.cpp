#include "service.hpp"
#include "auth.hpp"
#include "matching.hpp"
#include "stability.hpp"
#include <algorithm>
#include <chrono>
#include <limits>
#include <map>
#include <set>
#include <sstream>

namespace api {
namespace {
Id now() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
void need(bool condition, int status, const std::string &message) {
    if (!condition) {
        throw HttpError(status, message);
    }
}
std::string text(const Json &body, const std::string &key, std::size_t max = 500,
                 bool optional = false) {
    if (optional && !body.contains(key)) {
        return "";
    }
    need(body.contains(key) && body[key].is_string(), 400, key + " must be a string");
    const auto value = body[key].get<std::string>();
    need(value.size() <= max && value.find('\0') == std::string::npos, 400,
         key + " is invalid or too long");
    if (!optional) {
        need(value.find_first_not_of(" \t\r\n") != std::string::npos, 400,
             key + " cannot be blank");
    }
    return value;
}
Id number(const Json &value) {
    need(value.is_number_integer(), 400, "IDs and scores must be integers");
    if (value.is_number_unsigned()) {
        need(value.get<std::uint64_t>() <=
                 static_cast<std::uint64_t>(std::numeric_limits<Id>::max()),
             400, "Integer is too large");
    }
    const auto result = value.get<Id>();
    need(result > 0, 400, "IDs and scores must be positive");
    return result;
}
Id fieldId(const Json &body, const std::string &key) {
    need(body.contains(key), 400, "Missing " + key);
    return number(body[key]);
}
Id pathId(const std::string &value) {
    need(!value.empty() && value.size() <= 19 &&
             std::all_of(value.begin(), value.end(), [](char c) { return c >= '0' && c <= '9'; }),
         400, "Invalid path ID");
    try {
        const Id id = std::stoll(value);
        need(id > 0, 400, "Invalid path ID");
        return id;
    } catch (const std::out_of_range &) {
        throw HttpError(400, "Path ID is too large");
    }
}
void passwordRules(const std::string &password) {
    need(password.size() >= 12 && password.size() <= 256, 400,
         "Password must contain 12 to 256 bytes");
}
void emailRules(const std::string &email) {
    need(email.size() <= 254 && email.find('@') != std::string::npos &&
             email.find_first_of(" \t\r\n") == std::string::npos,
         400, "Invalid email");
}
std::vector<std::string> parts(const std::string &path) {
    std::vector<std::string> output;
    std::istringstream input(path);
    std::string part;
    while (std::getline(input, part, '/')) {
        if (!part.empty()) {
            output.push_back(part);
        }
    }
    return output;
}
std::string statusName(mentoring::VerificationStatus status) {
    switch (status) {
    case mentoring::VerificationStatus::Stable:
        return "Stable";
    case mentoring::VerificationStatus::Unstable:
        return "Unstable";
    case mentoring::VerificationStatus::InvalidAssignment:
        return "InvalidAssignment";
    case mentoring::VerificationStatus::InvalidPreferences:
        return "InvalidPreferences";
    }
    throw std::runtime_error("Unknown verifier status");
}
} // namespace

Service::Service(const std::string &database, const std::string &root, bool initialize_schema)
    : db_(database) {
    if (initialize_schema) {
        initialize(db_, root);
    }
    need(db_.scalar("PRAGMA user_version") == 2, 500,
         "Run with --init to initialize/migrate this database");
}
void Service::bootstrap(const std::string &email, const std::string &password) {
    emailRules(email);
    passwordRules(password);
    Transaction tx(db_);
    need(db_.scalar("SELECT count(*) FROM credentials") == 0, 409,
         "Bootstrap is disabled once credentials exist");
    Id user = 0;
    {
        auto q = db_.statement("SELECT id,is_admin,active FROM users WHERE email=? COLLATE NOCASE");
        q.bind(1, email);
        if (q.next()) {
            need(q.integer(1) == 1 && q.integer(2) == 1, 409,
                 "Existing bootstrap identity must be an active admin");
            user = q.integer(0);
        }
    }
    if (!user) {
        auto q = db_.statement(
            "INSERT INTO users(display_name,email,is_admin) VALUES('Administrator',?,1)");
        q.bind(1, email);
        q.execute();
        user = db_.lastId();
    }
    const auto hash = hashPassword(password);
    auto q = db_.statement("INSERT INTO credentials VALUES(?,?,?)");
    q.bind(1, user);
    q.bind(2, hash.salt);
    q.bind(3, hash.hash);
    q.execute();
    tx.commit();
}
Service::Actor Service::authenticate(const std::string &token) {
    need(token.size() == 64, 401, "Authentication required");
    auto q = db_.statement(
        "SELECT u.id,u.is_admin,u.display_name,u.email FROM sessions s "
        "JOIN users u ON u.id=s.user_id WHERE s.token_hash=? AND s.expires_at>? AND u.active=1");
    q.bind(1, tokenDigest(token));
    q.bind(2, now());
    need(q.next(), 401, "Invalid or expired session");
    return {q.integer(0), q.integer(1) == 1, q.text(2), q.text(3)};
}
void Service::adminOnly(const Actor &actor) {
    need(actor.admin, 403, "Administrator access required");
}
void Service::roundExists(Id round) {
    auto q = db_.statement("SELECT 1 FROM matching_rounds WHERE id=?");
    q.bind(1, round);
    need(q.next(), 404, "Round not found");
}
void Service::canViewRound(const Actor &actor, Id round) {
    roundExists(round);
    if (actor.admin) {
        return;
    }
    auto q = db_.statement("SELECT 1 FROM participants WHERE round_id=? AND user_id=?");
    q.bind(1, round);
    q.bind(2, actor.id);
    need(q.next(), 403, "Not a participant in this round");
}
Json Service::readiness(Id round) {
    auto q = db_.statement(
        "SELECT p.user_id,p.role,EXISTS(SELECT 1 FROM preference_versions v "
        "WHERE v.round_id=p.round_id AND v.owner_id=p.user_id AND v.state='submitted') "
        "FROM participants p WHERE p.round_id=? ORDER BY p.user_id");
    q.bind(1, round);
    Id mentors = 0, mentees = 0;
    Json missing = Json::array();
    while (q.next()) {
        if (q.text(1) == "mentor") {
            ++mentors;
        } else {
            ++mentees;
        }
        if (q.integer(2) == 0) {
            missing.push_back(q.integer(0));
        }
    }
    auto state = db_.statement("SELECT state FROM matching_rounds WHERE id=?");
    state.bind(1, round);
    state.next();
    return {{"mentors", mentors},
            {"mentees", mentees},
            {"missing_preferences", missing},
            {"ready", state.text(0) == "frozen" && mentors >= mentees && missing.empty()}};
}
Json Service::runDetails(const Actor &actor, Id run) {
    auto q = db_.statement(
        "SELECT r.round_id,r.state,r.kind,r.proposal_count,COALESCE(v.status,''),"
        "COALESCE(v.explanation,''),f.inputs_are_current,r.source_run_id FROM matching_runs r "
        "LEFT JOIN verification_results v ON v.run_id=r.id JOIN run_freshness f ON f.run_id=r.id "
        "WHERE r.id=?");
    q.bind(1, run);
    need(q.next(), 404, "Matching run not found");
    canViewRound(actor, q.integer(0));
    need(actor.admin || q.text(1) == "verified", 403,
         "Only verified assignments are visible to participants");
    Json output = {{"id", run},
                   {"round_id", q.integer(0)},
                   {"state", q.text(1)},
                   {"kind", q.text(2)},
                   {"inputs_are_current", q.integer(6) == 1},
                   {"source_run_id", q.integer(7) > 0 ? Json(q.integer(7)) : Json(nullptr)}};
    if (actor.admin) {
        output["proposal_count"] = q.integer(3);
        output["verification"] = {{"status", q.text(4)}, {"explanation", q.text(5)}};
        auto witness =
            db_.statement("SELECT blocking_mentor_id,blocking_mentee_id FROM verification_results "
                          "WHERE run_id=? AND status='Unstable'");
        witness.bind(1, run);
        if (witness.next()) {
            output["verification"]["blocking_pair"] = {{"mentor_id", witness.integer(0)},
                                                       {"mentee_id", witness.integer(1)}};
        }
    }
    auto a = db_.statement(
        "SELECT id,mentor_id,mentee_id FROM assignments WHERE run_id=? ORDER BY mentee_id");
    a.bind(1, run);
    output["assignments"] = Json::array();
    while (a.next()) {
        if (actor.admin || a.integer(1) == actor.id || a.integer(2) == actor.id) {
            output["assignments"].push_back(
                {{"id", a.integer(0)}, {"mentor_id", a.integer(1)}, {"mentee_id", a.integer(2)}});
        }
    }
    return output;
}

Json Service::createRun(const Actor &actor, Id round, const Json &body, bool recheck) {
    adminOnly(actor);
    Transaction tx(db_);
    roundExists(round);
    const auto ready = readiness(round);
    need(ready["ready"].get<bool>(), 409,
         "Round must be frozen with submitted preferences from everyone");
    std::vector<Id> mentor_ids, mentee_ids;
    std::map<Id, Id> versions;
    {
        auto q = db_.statement(
            "SELECT p.user_id,p.role,(SELECT id FROM preference_versions v "
            "WHERE v.round_id=p.round_id AND v.owner_id=p.user_id AND v.state='submitted' "
            "ORDER BY revision DESC LIMIT 1) FROM participants p WHERE p.round_id=? ORDER BY "
            "p.user_id");
        q.bind(1, round);
        while (q.next()) {
            (q.text(1) == "mentor" ? mentor_ids : mentee_ids).push_back(q.integer(0));
            versions[q.integer(0)] = q.integer(2);
        }
    }
    std::map<Id, std::size_t> mentor_index, mentee_index;
    for (std::size_t i = 0; i < mentor_ids.size(); ++i) {
        mentor_index[mentor_ids[i]] = i;
    }
    for (std::size_t i = 0; i < mentee_ids.size(); ++i) {
        mentee_index[mentee_ids[i]] = i;
    }
    auto load = [&](const std::vector<Id> &owners, const std::map<Id, std::size_t> &opposite) {
        mentoring::PreferenceLists lists;
        for (Id owner : owners) {
            std::vector<std::size_t> row;
            auto q = db_.statement(
                "SELECT target_id FROM preference_entries WHERE version_id=? ORDER BY rank");
            q.bind(1, versions.at(owner));
            while (q.next()) {
                row.push_back(opposite.at(q.integer(0)));
            }
            lists.push_back(row);
        }
        return lists;
    };
    const auto mentors = load(mentor_ids, mentee_index);
    const auto mentees = load(mentee_ids, mentor_index);
    mentoring::MatchingResult result;
    Id source_run = 0;
    if (!recheck) {
        result = mentoring::matchMentors(mentors, mentees);
    } else {
        const Id source = fieldId(body, "run_id");
        source_run = source;
        auto q = db_.statement("SELECT round_id,state FROM matching_runs WHERE id=?");
        q.bind(1, source);
        need(q.next(), 404, "Source run not found");
        need(q.integer(0) == round && q.text(1) == "verified", 409,
             "Recheck source must be a verified run in this round");
        result.mentor_to_mentee.resize(mentor_ids.size());
        result.mentee_to_mentor.resize(mentee_ids.size());
        auto a = db_.statement("SELECT mentor_id,mentee_id FROM assignments WHERE run_id=?");
        a.bind(1, source);
        while (a.next()) {
            auto m = mentor_index.at(a.integer(0)), n = mentee_index.at(a.integer(1));
            result.mentor_to_mentee[m] = n;
            result.mentee_to_mentor[n] = m;
        }
    }
    const auto verified = mentoring::verifyStability(mentors, mentees, result);
    auto insert = db_.statement(
        "INSERT INTO matching_runs(round_id,requested_by,kind,source_run_id) VALUES(?,?,?,?)");
    insert.bind(1, round);
    insert.bind(2, actor.id);
    insert.bind(3, std::string(recheck ? "recheck" : "matching"));
    if (recheck) {
        insert.bind(4, source_run);
    } else {
        insert.bindNull(4);
    }
    insert.execute();
    const Id run = db_.lastId();
    for (const auto &entry : versions) {
        auto q = db_.statement("INSERT INTO run_preferences VALUES(?,?,?,?)");
        q.bind(1, run);
        q.bind(2, round);
        q.bind(3, entry.first);
        q.bind(4, entry.second);
        q.execute();
    }
    for (std::size_t mentee = 0; mentee < result.mentee_to_mentor.size(); ++mentee) {
        if (result.mentee_to_mentor[mentee]) {
            auto q = db_.statement(
                "INSERT INTO assignments(run_id,round_id,mentor_id,mentee_id) VALUES(?,?,?,?)");
            q.bind(1, run);
            q.bind(2, round);
            q.bind(3, mentor_ids.at(*result.mentee_to_mentor[mentee]));
            q.bind(4, mentee_ids.at(mentee));
            q.execute();
        }
    }
    // Store permanent user IDs in explanations/witnesses, not local vector indices.
    std::string explanation = verified.explanation;
    if (verified.blocking_pair) {
        explanation = "Blocking pair: Mentor user " +
                      std::to_string(mentor_ids.at(verified.blocking_pair->mentor)) +
                      " and Mentee user " +
                      std::to_string(mentee_ids.at(verified.blocking_pair->mentee));
        auto q = db_.statement("INSERT INTO "
                               "verification_results(run_id,round_id,status,explanation,blocking_"
                               "mentor_id,blocking_mentee_id) VALUES(?,?,?,?,?,?)");
        q.bind(1, run);
        q.bind(2, round);
        q.bind(3, statusName(verified.status));
        q.bind(4, explanation);
        q.bind(5, mentor_ids.at(verified.blocking_pair->mentor));
        q.bind(6, mentee_ids.at(verified.blocking_pair->mentee));
        q.execute();
    } else {
        auto q = db_.statement(
            "INSERT INTO verification_results(run_id,round_id,status,explanation) VALUES(?,?,?,?)");
        q.bind(1, run);
        q.bind(2, round);
        q.bind(3, statusName(verified.status));
        q.bind(4, explanation);
        q.execute();
    }
    auto finish = db_.statement("UPDATE matching_runs SET state=?,proposal_count=? WHERE id=?");
    finish.bind(1, std::string(verified.isReasonable() ? "verified" : "rejected"));
    finish.bind(2, static_cast<Id>(result.proposal_count));
    finish.bind(3, run);
    finish.execute();
    tx.commit();
    return runDetails(actor, run);
}

Response Service::handle(const std::string &method, const std::string &path, const Json &body,
                         const std::string &token, const std::string &remote) {
    std::lock_guard<std::mutex> lock(mutex_);
    need(body.is_object(), 400, "JSON body must be an object");
    const auto route = parts(path);
    if (method == "GET" && path == "/api/health") {
        return {200, {{"status", "ok"}, {"schema_version", 2}}};
    }
    if (method == "POST" && path == "/api/auth/login") {
        const Id current_time = now();
        for (auto it = login_attempts_.begin(); it != login_attempts_.end();) {
            if (it->second.empty() || it->second.back() + 60 < current_time) {
                it = login_attempts_.erase(it);
            } else {
                ++it;
            }
        }
        need(login_attempts_.size() < 1024 || login_attempts_.count(remote), 429,
             "Login temporarily limited");
        auto &attempts = login_attempts_[remote];
        while (!attempts.empty() && attempts.front() + 60 < current_time) {
            attempts.pop_front();
        }
        need(attempts.size() < 20, 429, "Too many login attempts; wait one minute");
        attempts.push_back(current_time);
        const auto email = text(body, "email", 254), password = text(body, "password", 256);
        auto q = db_.statement(
            "SELECT u.id,c.salt,c.password_hash FROM users u JOIN credentials c ON c.user_id=u.id "
            "WHERE u.email=? COLLATE NOCASE AND u.active=1");
        q.bind(1, email);
        const bool found = q.next();
        const auto salt = found ? q.text(1) : std::string(32, '0');
        const auto hash = found ? q.text(2) : std::string(64, '0');
        const bool correct = checkPassword(password, salt, hash);
        need(found && correct, 401, "Invalid email or password");
        const Id user = q.integer(0), expiry = current_time + 8 * 60 * 60;
        const auto session = randomToken();
        db_.exec("DELETE FROM sessions WHERE expires_at <= CAST(strftime('%s','now') AS INTEGER)");
        auto add = db_.statement("INSERT INTO sessions VALUES(?,?,?)");
        add.bind(1, tokenDigest(session));
        add.bind(2, user);
        add.bind(3, expiry);
        add.execute();
        return {200, {{"token", session}, {"token_type", "Bearer"}, {"expires_at", expiry}}};
    }
    const auto actor = authenticate(token);
    if (method == "POST" && path == "/api/auth/logout") {
        auto q = db_.statement("DELETE FROM sessions WHERE token_hash=?");
        q.bind(1, tokenDigest(token));
        q.execute();
        return {200, {{"logged_out", true}}};
    }
    if (method == "GET" && path == "/api/me") {
        return {200,
                {{"id", actor.id},
                 {"name", actor.name},
                 {"email", actor.email},
                 {"is_admin", actor.admin}}};
    }
    if (method == "POST" && path == "/api/me/password") {
        const auto old = text(body, "current_password", 256),
                   replacement = text(body, "new_password", 256);
        passwordRules(replacement);
        Transaction tx(db_);
        auto q = db_.statement("SELECT salt,password_hash FROM credentials WHERE user_id=?");
        q.bind(1, actor.id);
        need(q.next() && checkPassword(old, q.text(0), q.text(1)), 401,
             "Current password is incorrect");
        const auto hash = hashPassword(replacement);
        auto update =
            db_.statement("UPDATE credentials SET salt=?,password_hash=? WHERE user_id=?");
        update.bind(1, hash.salt);
        update.bind(2, hash.hash);
        update.bind(3, actor.id);
        update.execute();
        auto revoke = db_.statement("DELETE FROM sessions WHERE user_id=?");
        revoke.bind(1, actor.id);
        revoke.execute();
        tx.commit();
        return {200, {{"password_changed", true}, {"login_required", true}}};
    }
    if (path == "/api/users" && method == "POST") {
        adminOnly(actor);
        const auto name = text(body, "name", 100), email = text(body, "email", 254),
                   password = text(body, "password", 256);
        emailRules(email);
        passwordRules(password);
        need(!body.contains("is_admin") || body["is_admin"].is_boolean(), 400,
             "is_admin must be boolean");
        const bool is_admin = body.value("is_admin", false);
        const auto hash = hashPassword(password);
        Transaction tx(db_);
        auto q = db_.statement("INSERT INTO users(display_name,email,is_admin) VALUES(?,?,?)");
        q.bind(1, name);
        q.bind(2, email);
        q.bind(3, static_cast<Id>(is_admin));
        q.execute();
        const Id user = db_.lastId();
        auto c = db_.statement("INSERT INTO credentials VALUES(?,?,?)");
        c.bind(1, user);
        c.bind(2, hash.salt);
        c.bind(3, hash.hash);
        c.execute();
        tx.commit();
        return {201, {{"id", user}, {"name", name}, {"email", email}, {"is_admin", is_admin}}};
    }
    if (path == "/api/users" && method == "GET") {
        adminOnly(actor);
        Json users = Json::array();
        auto q =
            db_.statement("SELECT id,display_name,email,is_admin,active FROM users ORDER BY id");
        while (q.next()) {
            users.push_back({{"id", q.integer(0)},
                             {"name", q.text(1)},
                             {"email", q.text(2)},
                             {"is_admin", q.integer(3) == 1},
                             {"active", q.integer(4) == 1}});
        }
        return {200, {{"users", users}}};
    }
    if (route.size() == 3 && route[0] == "api" && route[1] == "users" && method == "PATCH") {
        adminOnly(actor);
        const Id user = pathId(route[2]);
        need(user != actor.id, 409, "Cannot change your own active status");
        need(body.contains("active") && body["active"].is_boolean(), 400, "active must be boolean");
        auto exists = db_.statement("SELECT 1 FROM users WHERE id=?");
        exists.bind(1, user);
        need(exists.next(), 404, "User not found");
        Transaction tx(db_);
        auto q = db_.statement("UPDATE users SET active=? WHERE id=?");
        q.bind(1, static_cast<Id>(body["active"].get<bool>()));
        q.bind(2, user);
        q.execute();
        auto revoke = db_.statement("DELETE FROM sessions WHERE user_id=?");
        revoke.bind(1, user);
        revoke.execute();
        tx.commit();
        return {200, {{"id", user}, {"active", body["active"]}}};
    }
    if (path == "/api/rounds" && method == "POST") {
        adminOnly(actor);
        auto q = db_.statement("INSERT INTO matching_rounds(name) VALUES(?)");
        q.bind(1, text(body, "name", 100));
        q.execute();
        return {201, {{"id", db_.lastId()}, {"state", "collecting"}}};
    }
    if (path == "/api/rounds" && method == "GET") {
        Json rounds = Json::array();
        auto q = db_.statement("SELECT id,name,state FROM matching_rounds WHERE ?=1 OR "
                               "EXISTS(SELECT 1 FROM participants "
                               "WHERE round_id=matching_rounds.id AND user_id=?) ORDER BY id");
        q.bind(1, static_cast<Id>(actor.admin));
        q.bind(2, actor.id);
        while (q.next()) {
            rounds.push_back({{"id", q.integer(0)}, {"name", q.text(1)}, {"state", q.text(2)}});
        }
        return {200, {{"rounds", rounds}}};
    }
    if (route.size() >= 3 && route[0] == "api" && route[1] == "rounds") {
        const Id round = pathId(route[2]);
        canViewRound(actor, round);
        if (route.size() == 4 && route[3] == "participants" && method == "POST") {
            adminOnly(actor);
            auto count = db_.statement("SELECT count(*) FROM participants WHERE round_id=?");
            count.bind(1, round);
            count.next();
            need(count.integer(0) < 2000, 409, "This version limits a round to 2000 participants");
            const auto role = text(body, "role", 10);
            need(role == "mentor" || role == "mentee", 400, "role must be mentor or mentee");
            auto q = db_.statement("INSERT INTO participants VALUES(?,?,?)");
            q.bind(1, round);
            q.bind(2, fieldId(body, "user_id"));
            q.bind(3, role);
            q.execute();
            return {201, {{"added", true}}};
        }
        if (route.size() == 5 && route[3] == "participants" && method == "DELETE") {
            adminOnly(actor);
            const Id user = pathId(route[4]);
            auto exists =
                db_.statement("SELECT 1 FROM participants WHERE round_id=? AND user_id=?");
            exists.bind(1, round);
            exists.bind(2, user);
            need(exists.next(), 404, "Participant not found");
            auto q = db_.statement("DELETE FROM participants WHERE round_id=? AND user_id=?");
            q.bind(1, round);
            q.bind(2, user);
            q.execute();
            return {200, {{"removed", true}}};
        }
        if (route.size() == 4 && route[3] == "participants" && method == "GET") {
            Json participants = Json::array();
            auto q =
                db_.statement("SELECT p.user_id,u.display_name,p.role FROM participants p JOIN "
                              "users u ON u.id=p.user_id WHERE p.round_id=? ORDER BY p.user_id");
            q.bind(1, round);
            while (q.next()) {
                participants.push_back(
                    {{"user_id", q.integer(0)}, {"name", q.text(1)}, {"role", q.text(2)}});
            }
            return {200, {{"participants", participants}}};
        }
        if (route.size() == 4 && (route[3] == "freeze" || route[3] == "close") &&
            method == "POST") {
            adminOnly(actor);
            auto q = db_.statement("UPDATE matching_rounds SET state=? WHERE id=?");
            const std::string state = route[3] == "freeze" ? "frozen" : "closed";
            q.bind(1, state);
            q.bind(2, round);
            q.execute();
            return {200, {{"id", round}, {"state", state}}};
        }
        if (route.size() == 4 && route[3] == "readiness" && method == "GET") {
            adminOnly(actor);
            return {200, readiness(round)};
        }
        if (route.size() == 4 && route[3] == "preferences-overview" && method == "GET") {
            adminOnly(actor);
            Json people = Json::array();
            auto q = db_.statement("SELECT p.user_id,u.display_name,p.role,(SELECT id FROM preference_versions v WHERE v.round_id=p.round_id AND v.owner_id=p.user_id AND v.state='submitted' ORDER BY revision DESC LIMIT 1) FROM participants p JOIN users u ON u.id=p.user_id WHERE p.round_id=? ORDER BY p.user_id");
            q.bind(1, round);
            while (q.next()) {
                Json row = {{"user_id", q.integer(0)}, {"name", q.text(1)}, {"role", q.text(2)}, {"targets", nullptr}};
                if (q.integer(3) > 0) {
                    row["targets"] = Json::array();
                    auto entries = db_.statement("SELECT target_id FROM preference_entries WHERE version_id=? ORDER BY rank");
                    entries.bind(1, q.integer(3));
                    while (entries.next()) row["targets"].push_back(entries.integer(0));
                }
                people.push_back(row);
            }
            auto ready = readiness(round);
            return {200, {{"participants", people}, {"ready", ready["ready"]}, {"missing", ready["missing_preferences"].size()}}};
        }
        if (route.size() == 4 && route[3] == "preferences" && method == "PUT") {
            need(body.contains("targets") && body["targets"].is_array(), 400,
                 "targets must be an array of user IDs");
            need(body["targets"].size() <= 2000, 400, "Too many preference entries");
            std::vector<Id> targets;
            std::set<Id> unique;
            for (const auto &target : body["targets"]) {
                const Id id = number(target);
                need(unique.insert(id).second, 400, "Duplicate preference target");
                targets.push_back(id);
            }
            const auto reason = text(body, "reason", 2000, true);
            Transaction tx(db_);
            auto next = db_.statement("SELECT COALESCE(max(revision),0)+1 FROM preference_versions "
                                      "WHERE round_id=? AND owner_id=?");
            next.bind(1, round);
            next.bind(2, actor.id);
            next.next();
            auto q = db_.statement(
                "INSERT INTO preference_versions(round_id,owner_id,revision,change_reason) "
                "VALUES(?,?,?,?)");
            q.bind(1, round);
            q.bind(2, actor.id);
            q.bind(3, next.integer(0));
            q.bind(4, reason);
            q.execute();
            const Id version = db_.lastId();
            for (std::size_t rank = 0; rank < targets.size(); ++rank) {
                auto e = db_.statement("INSERT INTO preference_entries VALUES(?,?,?)");
                e.bind(1, version);
                e.bind(2, targets[rank]);
                e.bind(3, static_cast<Id>(rank + 1));
                e.execute();
            }
            auto submit =
                db_.statement("UPDATE preference_versions SET state='submitted' WHERE id=?");
            submit.bind(1, version);
            submit.execute();
            tx.commit();
            return {201, {{"id", version}, {"revision", next.integer(0)}, {"state", "submitted"}}};
        }
        if (route.size() == 4 && route[3] == "preferences" && method == "GET") {
            auto q = db_.statement(
                "SELECT id,revision,COALESCE(change_reason,'') FROM preference_versions WHERE "
                "round_id=? AND owner_id=? AND state='submitted' ORDER BY revision DESC LIMIT 1");
            q.bind(1, round);
            q.bind(2, actor.id);
            need(q.next(), 404, "No submitted preferences for your account");
            Json result = {{"id", q.integer(0)},
                           {"revision", q.integer(1)},
                           {"reason", q.text(2)},
                           {"targets", Json::array()}};
            auto entries = db_.statement(
                "SELECT target_id FROM preference_entries WHERE version_id=? ORDER BY rank");
            entries.bind(1, q.integer(0));
            while (entries.next()) {
                result["targets"].push_back(entries.integer(0));
            }
            return {200, result};
        }
        if (route.size() == 4 && (route[3] == "matches" || route[3] == "rechecks") &&
            method == "POST") {
            return {201, createRun(actor, round, body, route[3] == "rechecks")};
        }
        if (route.size() == 4 && route[3] == "matches" && method == "GET") {
            adminOnly(actor);
            Json runs = Json::array();
            auto q = db_.statement("SELECT id FROM matching_runs WHERE round_id=? ORDER BY id");
            q.bind(1, round);
            while (q.next()) {
                runs.push_back(runDetails(actor, q.integer(0)));
            }
            return {200, {{"runs", runs}}};
        }
        if (route.size() == 4 && route[3] == "my-assignment" && method == "GET") {
            auto q = db_.statement("SELECT id FROM matching_runs WHERE round_id=? AND "
                                   "state='verified' ORDER BY id DESC LIMIT 1");
            q.bind(1, round);
            need(q.next(), 404, "No verified matching yet");
            return {200, runDetails(actor, q.integer(0))};
        }
    }
    if (route.size() == 3 && route[0] == "api" && route[1] == "matches" && method == "GET") {
        return {200, runDetails(actor, pathId(route[2]))};
    }
    if (path == "/api/ratings" && method == "POST") {
        const Id assignment = fieldId(body, "assignment_id"), score = fieldId(body, "score");
        need(score <= 5, 400, "score must be 1 through 5");
        const auto comment = text(body, "comment", 2000, true);
        auto a = db_.statement("SELECT mentor_id,mentee_id FROM assignments WHERE id=?");
        a.bind(1, assignment);
        need(a.next(), 404, "Assignment not found");
        need(a.integer(0) == actor.id || a.integer(1) == actor.id, 403,
             "Only assigned partners may submit ratings");
        const Id target = a.integer(0) == actor.id ? a.integer(1) : a.integer(0);
        auto q =
            db_.statement("INSERT INTO ratings(assignment_id,author_id,target_id,score,comment) "
                          "VALUES(?,?,?,?,?)");
        q.bind(1, assignment);
        q.bind(2, actor.id);
        q.bind(3, target);
        q.bind(4, score);
        q.bind(5, comment);
        q.execute();
        return {201, {{"id", db_.lastId()}, {"submitted", true}}};
    }
    if (path == "/api/ratings" && method == "GET") {
        adminOnly(actor);
        Json ratings = Json::array();
        auto q = db_.statement(
            "SELECT id,assignment_id,author_id,target_id,score,comment FROM ratings ORDER BY id");
        while (q.next()) {
            ratings.push_back({{"id", q.integer(0)},
                               {"assignment_id", q.integer(1)},
                               {"author_id", q.integer(2)},
                               {"target_id", q.integer(3)},
                               {"score", q.integer(4)},
                               {"comment", q.text(5)}});
        }
        return {200, {{"ratings", ratings}}};
    }
    throw HttpError(404, "Endpoint not found");
}
} // namespace api
