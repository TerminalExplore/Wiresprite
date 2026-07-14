#include "auth/auth.hpp"
#include "doctest.h"

using namespace wiresprite;

TEST_CASE("sha256Hex matches known test vectors") {
    // Cross-checked against Python's hashlib (an independent SHA-256
    // implementation) rather than hand-typed from memory — the "abc"
    // vector is also the standard NIST example.
    CHECK(sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256Hex("password") == "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8");
}

TEST_CASE("SessionAuth: disabled when passwordHash is empty") {
    SessionAuth auth("admin", "", 60);
    CHECK_FALSE(auth.enabled());
    CHECK(auth.checkCredentials("admin", "wrong-password"));
    CHECK(auth.checkCredentials("anyone", "anything")); // disabled means every attempt is "authorized"
}

TEST_CASE("SessionAuth: checkCredentials against a configured hash") {
    SessionAuth auth("admin", sha256Hex("correct-horse"), 60);
    CHECK(auth.enabled());
    CHECK(auth.checkCredentials("admin", "correct-horse"));
    CHECK_FALSE(auth.checkCredentials("admin", "wrong-password"));
    CHECK_FALSE(auth.checkCredentials("someone-else", "correct-horse"));
    CHECK_FALSE(auth.checkCredentials("admin", ""));
}

TEST_CASE("SessionAuth: createSession / isValidSession / destroySession") {
    SessionAuth auth("admin", sha256Hex("hunter2"), 60);

    std::string token = auth.createSession();
    CHECK(token.size() == 32); // 128-bit token, hex-encoded
    CHECK(auth.isValidSession(token));
    CHECK_FALSE(auth.isValidSession("not-a-real-token"));
    CHECK_FALSE(auth.isValidSession(""));

    auth.destroySession(token);
    CHECK_FALSE(auth.isValidSession(token));

    // Destroying an unknown token is a safe no-op, not an error.
    auth.destroySession("never-existed");
}

TEST_CASE("SessionAuth: two sessions get distinct tokens") {
    SessionAuth auth("admin", sha256Hex("hunter2"), 60);
    std::string a = auth.createSession();
    std::string b = auth.createSession();
    CHECK(a != b);
    CHECK(auth.isValidSession(a));
    CHECK(auth.isValidSession(b));
}

TEST_CASE("SessionAuth: a session with a zero-minute TTL is immediately expired") {
    SessionAuth auth("admin", sha256Hex("hunter2"), 0);
    std::string token = auth.createSession();
    CHECK_FALSE(auth.isValidSession(token));
}

TEST_CASE("SessionAuth: setCredentials takes effect immediately, without a restart") {
    SessionAuth auth("admin", "", 60); // starts disabled, as first-run leaves it
    CHECK_FALSE(auth.enabled());

    auth.setCredentials("alice", sha256Hex("new-password"));

    CHECK(auth.enabled());
    CHECK(auth.checkCredentials("alice", "new-password"));
    CHECK_FALSE(auth.checkCredentials("admin", "new-password")); // old username no longer valid
}

TEST_CASE("SessionAuth: remember-me sessions use rememberMeDays, not sessionTtlMinutes") {
    // A 0-minute normal TTL expires instantly (as covered above); a
    // remembered session with a real rememberMeDays should survive.
    SessionAuth auth("admin", sha256Hex("hunter2"), /*sessionTtlMinutes=*/0, /*rememberMeDays=*/30);

    std::string normal = auth.createSession(false);
    CHECK_FALSE(auth.isValidSession(normal));

    std::string remembered = auth.createSession(true);
    CHECK(auth.isValidSession(remembered));
}
