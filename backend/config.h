#pragma once
#include <cstdlib>
#include <string>

// Central place for configuration read from environment variables.
// Never hardcode secrets here — set them as environment variables
// in your local shell (development) or in Render's dashboard (production).
namespace config {

inline std::string getEnvOrDefault(const char* name, const std::string& fallback) {
    const char* val = std::getenv(name);
    if (val == nullptr || std::string(val).empty()) {
        return fallback;
    }
    return std::string(val);
}

// Path to the SQLite database file.
// On Render, point this at a mounted persistent disk, e.g. /data/bank.db
inline std::string dbPath() {
    return getEnvOrDefault("BANK_DB_PATH", "./bank.db");
}

// Port the HTTP server listens on. Render injects PORT automatically.
inline int port() {
    return std::stoi(getEnvOrDefault("PORT", "8080"));
}

// Comma-separated list of allowed CORS origins for production,
// e.g. "https://yourusername.github.io"
// Use "*" only for local development.
inline std::string allowedOrigins() {
    return getEnvOrDefault("ALLOWED_ORIGINS", "*");
}

// Secret used only to make session tokens harder to guess (defense in depth).
// The actual token is a large random value; this pepper is mixed into
// password hashing so that a stolen database alone is not enough to
// brute-force passwords offline as quickly.
inline std::string passwordPepper() {
    return getEnvOrDefault("PASSWORD_PEPPER", "dev-only-pepper-change-me");
}

// Session lifetime in hours.
inline int sessionLifetimeHours() {
    return std::stoi(getEnvOrDefault("SESSION_LIFETIME_HOURS", "12"));
}

// Max failed login attempts allowed per email within the lockout window.
inline int maxLoginAttempts() {
    return std::stoi(getEnvOrDefault("MAX_LOGIN_ATTEMPTS", "5"));
}

// Lockout window in minutes.
inline int loginLockoutWindowMinutes() {
    return std::stoi(getEnvOrDefault("LOGIN_LOCKOUT_WINDOW_MINUTES", "15"));
}

} // namespace config
