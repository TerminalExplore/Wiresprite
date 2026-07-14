#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace wiresprite {

// Hashes `input` with SHA-256 and returns the lowercase hex digest —
// the format AuthConfig::passwordHash expects (e.g. from
// `printf '%s' "your-password" | sha256sum`).
std::string sha256Hex(const std::string& input);

// Session-cookie auth guarding the dashboard: checks a username/
// password against configured credentials, then issues and validates
// opaque session tokens. This is a local-admin-dashboard login, not a
// public-facing auth system — there's no TLS anywhere in this project,
// so cookies are HttpOnly/SameSite=Lax but not Secure, and sessions
// are in-memory only (lost on restart, which is fine for this use).
class SessionAuth {
public:
    // If `passwordHash` is empty, auth is disabled: enabled() is false
    // and every request should be treated as already authorized. This
    // is a deliberate config choice (no password_hash set), not a
    // fallback to guess at — HttpServer logs it plainly at startup.
    SessionAuth(std::string username, std::string passwordHash, int sessionTtlMinutes);

    bool enabled() const { return !passwordHash_.empty(); }

    // Constant-time comparison against the configured hash, so a wrong
    // guess can't be timed to learn how many leading hex digits matched.
    bool checkCredentials(const std::string& username, const std::string& password) const;

    // Creates a new session (also opportunistically prunes expired
    // ones) and returns its opaque token.
    std::string createSession();

    bool isValidSession(const std::string& token) const;

    void destroySession(const std::string& token);

private:
    void pruneExpiredLocked();

    std::string username_;
    std::string passwordHash_;
    std::chrono::minutes sessionTtl_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> sessions_; // token -> expiry
};

} // namespace wiresprite
