#include "auth/auth.hpp"

#include <cstdio>
#include <random>

#include "picosha2.h"

namespace wiresprite {

namespace {

// Constant-time-ish comparison: always walks the full length of the
// longer string, and folds every byte comparison into the same
// accumulator regardless of where a mismatch occurs, so equality
// checking doesn't leak how many leading bytes matched via timing.
bool constantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        // Still touch b so the false path isn't trivially fast, though
        // a length mismatch alone doesn't leak anything sensitive here
        // (hash length is fixed; this only matters if that changes).
        volatile unsigned char sink = 0;
        for (unsigned char c : b) {
            sink ^= c;
        }
        return false;
    }
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return diff == 0;
}

std::string randomSessionToken() {
    std::random_device rd;
    std::mt19937_64 rng(rd());
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t high = dist(rng);
    uint64_t low = dist(rng);

    char buf[33];
    std::snprintf(buf, sizeof(buf), "%016llx%016llx", static_cast<unsigned long long>(high),
                  static_cast<unsigned long long>(low));
    return std::string(buf, 32);
}

} // namespace

std::string sha256Hex(const std::string& input) {
    return picosha2::hash256_hex_string(input);
}

SessionAuth::SessionAuth(std::string username, std::string passwordHash, int sessionTtlMinutes)
    : username_(std::move(username)), passwordHash_(std::move(passwordHash)), sessionTtl_(sessionTtlMinutes) {}

bool SessionAuth::checkCredentials(const std::string& username, const std::string& password) const {
    if (!enabled()) {
        return true;
    }
    // Username isn't secret (it's one configured admin account), so a
    // plain compare is fine; only the password hash needs to be
    // constant-time.
    if (username != username_) {
        return false;
    }
    return constantTimeEquals(sha256Hex(password), passwordHash_);
}

std::string SessionAuth::createSession() {
    std::lock_guard<std::mutex> lock(mutex_);
    pruneExpiredLocked();

    std::string token = randomSessionToken();
    sessions_[token] = std::chrono::steady_clock::now() + sessionTtl_;
    return token;
}

bool SessionAuth::isValidSession(const std::string& token) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(token);
    if (it == sessions_.end()) {
        return false;
    }
    return std::chrono::steady_clock::now() < it->second;
}

void SessionAuth::destroySession(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(token);
}

void SessionAuth::pruneExpiredLocked() {
    auto now = std::chrono::steady_clock::now();
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (it->second <= now) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace wiresprite
