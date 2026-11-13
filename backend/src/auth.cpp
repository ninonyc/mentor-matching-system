#include "auth.hpp"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <array>
#include <stdexcept>
#include <vector>

namespace api {
namespace {
std::string hex(const unsigned char *bytes, std::size_t count) {
    constexpr char digits[] = "0123456789abcdef";
    std::string output;
    output.reserve(count * 2);
    for (std::size_t i = 0; i < count; ++i) {
        output += digits[bytes[i] >> 4];
        output += digits[bytes[i] & 15];
    }
    return output;
}
std::vector<unsigned char> unhex(const std::string &text) {
    if (text.size() % 2) {
        throw std::runtime_error("Invalid password metadata");
    }
    std::vector<unsigned char> output;
    for (std::size_t i = 0; i < text.size(); i += 2) {
        auto digit = [](char c) -> unsigned char {
            if (c >= '0' && c <= '9') {
                return static_cast<unsigned char>(c - '0');
            }
            if (c >= 'a' && c <= 'f') {
                return static_cast<unsigned char>(c - 'a' + 10);
            }
            throw std::runtime_error("Invalid password metadata");
        };
        output.push_back(static_cast<unsigned char>((digit(text[i]) << 4) | digit(text[i + 1])));
    }
    return output;
}
std::array<unsigned char, 32> derive(const std::string &password,
                                     const std::vector<unsigned char> &salt) {
    std::array<unsigned char, 32> output{};
    // scrypt: 32 MiB working memory, parallelization 3; never a plain SHA password hash.
    if (EVP_PBE_scrypt(password.data(), password.size(), salt.data(), salt.size(), 32768, 8, 3,
                       64 * 1024 * 1024, output.data(), output.size()) != 1) {
        throw std::runtime_error("Password derivation failed");
    }
    return output;
}
} // namespace
PasswordHash hashPassword(const std::string &password) {
    std::vector<unsigned char> salt(16);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) {
        throw std::runtime_error("Random source failed");
    }
    auto hash = derive(password, salt);
    return {hex(salt.data(), salt.size()), hex(hash.data(), hash.size())};
}
bool checkPassword(const std::string &password, const std::string &salt, const std::string &hash) {
    auto expected = unhex(hash);
    auto computed = derive(password, unhex(salt));
    return expected.size() == computed.size() &&
           CRYPTO_memcmp(computed.data(), expected.data(), computed.size()) == 0;
}
std::string randomToken() {
    std::array<unsigned char, 32> bytes{};
    if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
        throw std::runtime_error("Random source failed");
    }
    return hex(bytes.data(), bytes.size());
}
std::string tokenDigest(const std::string &token) {
    std::array<unsigned char, EVP_MAX_MD_SIZE> bytes{};
    unsigned int count = 0;
    if (EVP_Digest(token.data(), token.size(), bytes.data(), &count, EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("Token digest failed");
    }
    return hex(bytes.data(), count);
}
} // namespace api
