#pragma once
#include <string>
namespace api {
struct PasswordHash {
    std::string salt;
    std::string hash;
};
PasswordHash hashPassword(const std::string &password);
bool checkPassword(const std::string &password, const std::string &salt, const std::string &hash);
std::string randomToken();
std::string tokenDigest(const std::string &token);
} // namespace api
