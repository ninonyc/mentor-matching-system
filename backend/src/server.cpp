#include "service.hpp"
#include "httplib.h"
#include <cstdlib>
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>

namespace {
std::string passwordInput() {
    termios original{};
    const bool terminal = isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &original) == 0;
    if (terminal) {
        auto hidden = original;
        hidden.c_lflag &= static_cast<tcflag_t>(~ECHO);
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &hidden) != 0) {
            throw std::runtime_error("Cannot hide password input");
        }
    }
    std::string password;
    std::getline(std::cin, password);
    if (terminal) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
        std::cerr << '\n';
    }
    return password;
}
} // namespace

int main(int argc, char *argv[]) {
    // New database/journal files are private to this operating-system user.
    umask(0077);
    std::string database = "build/mentoring.db", root = ".", admin_email;
    int port = 8080;
    bool initialize = false, migrate_only = false;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--init") {
                initialize = true;
            } else if (option == "--migrate-only") {
                migrate_only = true;
            } else if ((option == "--database" || option == "--project-root" ||
                        option == "--admin-email" || option == "--port") &&
                       i + 1 < argc) {
                const std::string value = argv[++i];
                if (option == "--database") {
                    database = value;
                } else if (option == "--project-root") {
                    root = value;
                } else if (option == "--admin-email") {
                    admin_email = value;
                } else {
                    port = std::stoi(value);
                    if (port < 1 || port > 65535) {
                        throw std::runtime_error("Invalid port");
                    }
                }
            } else {
                throw std::runtime_error(
                    "Usage: mentor_api [--init] [--migrate-only] [--database PATH] [--project-root "
                    "PATH] [--admin-email EMAIL] [--port PORT]");
            }
        }
        api::Service service(database, root, initialize);
        if (!admin_email.empty()) {
            std::cerr << "Initial administrator password (12+ bytes): " << std::flush;
            auto password = passwordInput();
            std::cerr << "Confirm password: " << std::flush;
            auto confirmation = passwordInput();
            if (password != confirmation) {
                throw std::runtime_error("Passwords do not match");
            }
            service.bootstrap(admin_email, password);
            std::fill(password.begin(), password.end(), '\0');
            std::fill(confirmation.begin(), confirmation.end(), '\0');
            std::cout << "Administrator created.\n";
        }
        if (migrate_only) {
            std::cout << "Schema version 2 ready.\n";
            return 0;
        }
        const char *configured_origin = std::getenv("MENTOR_ALLOWED_ORIGIN");
        const std::string origin = configured_origin ? configured_origin : "";
        httplib::Server server;
        // Allow immediate restarts without sharing an active listening port.
        server.set_socket_options([](auto socket) {
            httplib::set_socket_opt(socket, SOL_SOCKET, SO_REUSEADDR, 1);
        });
        server.set_error_handler([](const httplib::Request &, httplib::Response &response) {
            response.set_header("Cache-Control", "no-store");
            response.set_header("X-Content-Type-Options", "nosniff");
            if (response.get_header_value("Content-Type").rfind("application/json", 0) != 0) {
                response.set_content(
                    api::Json({{"error", response.status == 413 ? "Request body exceeds 64 KiB"
                                                                : "Request rejected"}})
                        .dump(),
                    "application/json");
            }
        });
        server.set_payload_max_length(65536);
        server.set_read_timeout(10);
        server.set_write_timeout(10);
        server.new_task_queue = [] { return new httplib::ThreadPool(2, 0, 64); };
        auto handle = [&](const httplib::Request &request, httplib::Response &response) {
            response.set_header("Cache-Control", "no-store");
            response.set_header("X-Content-Type-Options", "nosniff");
            try {
                const auto request_origin = request.get_header_value("Origin");
                if (!request_origin.empty()) {
                    if (origin.empty() || request_origin != origin) {
                        throw api::HttpError(403, "Origin is not allowed");
                    }
                    response.set_header("Access-Control-Allow-Origin", origin);
                    response.set_header("Vary", "Origin");
                }
                if (request.method == "OPTIONS") {
                    response.set_header("Access-Control-Allow-Methods",
                                        "GET,POST,PUT,PATCH,DELETE,OPTIONS");
                    response.set_header("Access-Control-Allow-Headers",
                                        "Authorization,Content-Type");
                    response.status = 204;
                    return;
                }
                api::Json body = api::Json::object();
                if (!request.body.empty()) {
                    const auto content_type = request.get_header_value("Content-Type");
                    if (content_type != "application/json" &&
                        content_type.rfind("application/json;", 0) != 0) {
                        throw api::HttpError(415, "Use application/json");
                    }
                    body = api::Json::parse(
                        request.body, [](int depth, api::Json::parse_event_t, api::Json &) {
                            if (depth > 32) {
                                throw api::HttpError(400, "JSON nesting exceeds 32 levels");
                            }
                            return true;
                        });
                }
                const auto authorization = request.get_header_value("Authorization");
                const std::string token =
                    authorization.rfind("Bearer ", 0) == 0 ? authorization.substr(7) : "";
                const auto result =
                    service.handle(request.method, request.path, body, token, request.remote_addr);
                response.status = result.status;
                response.set_content(result.body.dump(), "application/json");
            } catch (const api::HttpError &error) {
                response.status = error.status;
                response.set_content(api::Json({{"error", error.what()}}).dump(),
                                     "application/json");
            } catch (const api::Json::exception &) {
                response.status = 400;
                response.set_content("{\"error\":\"Malformed JSON or incorrect field types\"}",
                                     "application/json");
            } catch (const api::DbError &error) {
                const int primary = error.code & 0xff;
                if (primary == SQLITE_CONSTRAINT) {
                    response.status = 409;
                    response.set_content(api::Json({{"error", error.what()}}).dump(),
                                         "application/json");
                } else if (primary == SQLITE_BUSY || primary == SQLITE_LOCKED) {
                    response.status = 503;
                    response.set_content("{\"error\":\"Database busy; retry later\"}",
                                         "application/json");
                } else {
                    std::cerr << "Database operation failed (code " << error.code << ").\n";
                    response.status = 500;
                    response.set_content("{\"error\":\"Database operation failed\"}",
                                         "application/json");
                }
            } catch (const std::exception &) {
                std::cerr << "Request failed internally.\n";
                response.status = 500;
                response.set_content("{\"error\":\"Internal server error\"}", "application/json");
            }
        };
        server.Get(R"(/api/.*)", handle);
        server.Post(R"(/api/.*)", handle);
        server.Put(R"(/api/.*)", handle);
        server.Patch(R"(/api/.*)", handle);
        server.Delete(R"(/api/.*)", handle);
        server.Options(R"(/api/.*)", handle);
        std::cout << "Mentoring API listening on http://127.0.0.1:" << port << std::endl;
        if (!server.listen("127.0.0.1", port)) {
            throw std::runtime_error("Could not bind API port");
        }
    } catch (const std::exception &error) {
        std::cerr << "Startup failed: " << error.what() << '\n';
        return 1;
    }
}
