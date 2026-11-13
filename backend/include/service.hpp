#pragma once
#include "database.hpp"
#include "json.hpp"
#include <mutex>
#include <string>
#include <unordered_map>
#include <deque>

namespace api {
using Json = nlohmann::json;
struct HttpError : std::runtime_error {
    int status;
    HttpError(int value, const std::string &message) : std::runtime_error(message), status(value) {}
};
struct Response {
    int status;
    Json body;
};

class Service {
    Database db_;
    // One connection, serialized operations: simple transaction ownership.
    std::mutex mutex_;
    std::unordered_map<std::string, std::deque<Id>> login_attempts_;
    struct Actor {
        Id id;
        bool admin;
        std::string name;
        std::string email;
    };
    Actor authenticate(const std::string &token);
    void adminOnly(const Actor &actor);
    void roundExists(Id round);
    void canViewRound(const Actor &actor, Id round);
    Json runDetails(const Actor &actor, Id run);
    Json readiness(Id round);
    Json createRun(const Actor &actor, Id round, const Json &body, bool recheck);

  public:
    explicit Service(const std::string &database, const std::string &root, bool initialize_schema);
    void bootstrap(const std::string &email, const std::string &password);
    Response handle(const std::string &method, const std::string &path, const Json &body,
                    const std::string &token, const std::string &remote);
};
} // namespace api
