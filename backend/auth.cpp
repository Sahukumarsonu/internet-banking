#include "auth.h"
#include "database.h"
#include "config.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>
#include <regex>
#include <chrono>
#include <ctime>
#include <vector>
#include <stdexcept>

namespace {

std::string toHex(const unsigned char* data, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return oss.str();
}

std::string fromHexToBytes(const std::string& hex) {
    std::string bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        std::string byteStr = hex.substr(i, 2);
        bytes.push_back(static_cast<char>(std::stoi(byteStr, nullptr, 16)));
    }
    return bytes;
}

std::vector<unsigned char> randomBytes(size_t n) {
    std::vector<unsigned char> buf(n);
    if (RAND_bytes(buf.data(), static_cast<int>(n)) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }
    return buf;
}

constexpr int PBKDF2_ITERATIONS = 210000;
constexpr int SALT_LEN = 16;
constexpr int HASH_LEN = 32;

std::string nowIso() {
    auto t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::gmtime(&t));
    return std::string(buf);
}

std::string plusHoursIso(int hours) {
    auto t = std::time(nullptr) + hours * 3600;
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::gmtime(&t));
    return std::string(buf);
}

std::string plusMinutesAgoIso(int minutes) {
    auto t = std::time(nullptr) - minutes * 60;
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::gmtime(&t));
    return std::string(buf);
}

} // namespace

namespace auth {

std::string hashPassword(const std::string& plaintext) {
    auto salt = randomBytes(SALT_LEN);
    std::string peppered = plaintext + config::passwordPepper();

    unsigned char out[HASH_LEN];
    int rc = PKCS5_PBKDF2_HMAC(
        peppered.c_str(), static_cast<int>(peppered.size()),
        salt.data(), static_cast<int>(salt.size()),
        PBKDF2_ITERATIONS,
        EVP_sha256(),
        HASH_LEN, out
    );
    if (rc != 1) {
        throw std::runtime_error("PBKDF2 hashing failed");
    }

    std::ostringstream result;
    result << "pbkdf2-sha256$" << PBKDF2_ITERATIONS << "$"
           << toHex(salt.data(), salt.size()) << "$"
           << toHex(out, HASH_LEN);
    return result.str();
}

bool verifyPassword(const std::string& plaintext, const std::string& storedHash) {
    // Format: pbkdf2-sha256$<iterations>$<salt_hex>$<hash_hex>
    std::vector<std::string> parts;
    std::stringstream ss(storedHash);
    std::string part;
    while (std::getline(ss, part, '$')) parts.push_back(part);
    if (parts.size() != 4 || parts[0] != "pbkdf2-sha256") return false;

    int iterations = std::stoi(parts[1]);
    std::string salt = fromHexToBytes(parts[2]);
    std::string expectedHash = parts[3];

    std::string peppered = plaintext + config::passwordPepper();
    unsigned char out[HASH_LEN];
    int rc = PKCS5_PBKDF2_HMAC(
        peppered.c_str(), static_cast<int>(peppered.size()),
        reinterpret_cast<const unsigned char*>(salt.data()), static_cast<int>(salt.size()),
        iterations,
        EVP_sha256(),
        HASH_LEN, out
    );
    if (rc != 1) return false;

    std::string computedHex = toHex(out, HASH_LEN);

    // Constant-time comparison to avoid timing side channels.
    if (computedHex.size() != expectedHash.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < computedHex.size(); ++i) {
        diff |= static_cast<unsigned char>(computedHex[i]) ^ static_cast<unsigned char>(expectedHash[i]);
    }
    return diff == 0;
}

std::string generateSessionToken() {
    auto bytes = randomBytes(32);
    return toHex(bytes.data(), bytes.size());
}

std::string generateAccountNumber() {
    auto bytes = randomBytes(6);
    std::ostringstream oss;
    oss << "IBS";
    for (auto b : bytes) {
        oss << (static_cast<int>(b) % 10);
    }
    return oss.str();
}

std::string createSession(long long userId) {
    auto& db = Database::instance();
    std::lock_guard<std::mutex> lock(db.writeMutex());

    std::string token = generateSessionToken();
    std::string expires = plusHoursIso(config::sessionLifetimeHours());

    Statement stmt(db.handle(),
        "INSERT INTO sessions (token, user_id, expires_at) VALUES (?, ?, ?)");
    stmt.bindText(1, token);
    stmt.bindInt(2, userId);
    stmt.bindText(3, expires);
    stmt.execute();

    return token;
}

std::optional<SessionUser> getUserFromToken(const std::string& token) {
    if (token.empty()) return std::nullopt;
    auto& db = Database::instance();

    Statement stmt(db.handle(),
        "SELECT u.id, u.email, u.full_name, u.is_admin, u.is_active "
        "FROM sessions s JOIN users u ON u.id = s.user_id "
        "WHERE s.token = ? AND s.expires_at > datetime('now')");
    stmt.bindText(1, token);

    if (!stmt.step()) return std::nullopt;

    SessionUser user;
    user.userId = stmt.columnInt(0);
    user.email = stmt.columnText(1);
    user.fullName = stmt.columnText(2);
    user.isAdmin = stmt.columnInt(3) != 0;
    user.isActive = stmt.columnInt(4) != 0;
    return user;
}

void destroySession(const std::string& token) {
    auto& db = Database::instance();
    std::lock_guard<std::mutex> lock(db.writeMutex());
    Statement stmt(db.handle(), "DELETE FROM sessions WHERE token = ?");
    stmt.bindText(1, token);
    stmt.execute();
}

bool isLoginAllowed(const std::string& email) {
    auto& db = Database::instance();
    std::string windowStart = plusMinutesAgoIso(config::loginLockoutWindowMinutes());

    Statement stmt(db.handle(),
        "SELECT COUNT(*) FROM login_attempts "
        "WHERE email = ? AND success = 0 AND attempted_at > ?");
    stmt.bindText(1, email);
    stmt.bindText(2, windowStart);
    stmt.step();
    long long failures = stmt.columnInt(0);
    return failures < config::maxLoginAttempts();
}

void recordLoginAttempt(const std::string& email, bool success) {
    auto& db = Database::instance();
    std::lock_guard<std::mutex> lock(db.writeMutex());
    Statement stmt(db.handle(),
        "INSERT INTO login_attempts (email, success) VALUES (?, ?)");
    stmt.bindText(1, email);
    stmt.bindInt(2, success ? 1 : 0);
    stmt.execute();
}

bool isValidEmail(const std::string& email) {
    static const std::regex pattern(R"(^[^\s@]+@[^\s@]+\.[^\s@]+$)");
    return std::regex_match(email, pattern);
}

bool isValidPhone(const std::string& phone) {
    static const std::regex pattern(R"(^\+?[0-9]{7,15}$)");
    return std::regex_match(phone, pattern);
}

bool isStrongEnoughPassword(const std::string& password) {
    return password.size() >= 8;
}

} // namespace auth
