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
    SessionAuth(std::string username, std::string passwordHash, int sessionTtlMinutes, int rememberMeDays = 30);

    bool enabled() const;

    // Constant-time comparison against the configured hash, so a wrong
    // guess can't be timed to learn how many leading hex digits matched.
    bool checkCredentials(const std::string& username, const std::string& password) const;

    // Live credential update, used by the settings page (POST
    // /api/config) so a password change — including setting the very
    // first password during first-run setup — takes effect immediately
    // rather than requiring a restart like the rest of the config does.
    void setCredentials(std::string username, std::string passwordHash);

    // Creates a new session (also opportunistically prunes expired
    // ones) and returns its opaque token. `remember` uses the longer
    // rememberMeDays TTL instead of the normal sessionTtlMinutes one —
    // the caller (POST /login) still also needs to set a matching
    // cookie Max-Age, or a long-lived cookie would just get rejected
    // once the short server-side session expires anyway.
    std::string createSession(bool remember = false);

    // For the caller (POST /login) to set a matching cookie Max-Age
    // when `remember` was true — the cookie's lifetime and the
    // server-side session's TTL need to agree, so this is the single
    // source of truth for both.
    long long rememberMeTtlSeconds() const {
        return std::chrono::duration_cast<std::chrono::seconds>(rememberMeTtl_).count();
    }

    bool isValidSession(const std::string& token) const;

    void destroySession(const std::string& token);

private:
    void pruneExpiredLocked();

    std::string username_;
    std::string passwordHash_;
    std::chrono::minutes sessionTtl_;
    std::chrono::hours rememberMeTtl_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> sessions_; // token -> expiry
};

} // namespace wiresprite
