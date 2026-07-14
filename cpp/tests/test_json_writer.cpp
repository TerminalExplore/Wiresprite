#include "doctest.h"
#include "http/json_writer.hpp"

using namespace wiresprite::json;

namespace {
std::string escaped(const std::string& value) {
    std::string out;
    appendEscapedString(out, value);
    return out;
}
} // namespace

TEST_CASE("appendEscapedString passes plain ASCII through unchanged") {
    CHECK(escaped("eth0") == "\"eth0\"");
    CHECK(escaped("") == "\"\"");
    CHECK(escaped("HP ProCurve 2512") == "\"HP ProCurve 2512\"");
}

TEST_CASE("appendEscapedString escapes quotes and backslashes") {
    CHECK(escaped("say \"hi\"") == "\"say \\\"hi\\\"\"");
    CHECK(escaped("C:\\path") == "\"C:\\\\path\"");
}

TEST_CASE("appendEscapedString escapes the named control characters") {
    CHECK(escaped("a\nb") == "\"a\\nb\"");
    CHECK(escaped("a\tb") == "\"a\\tb\"");
    CHECK(escaped("a\rb") == "\"a\\rb\"");
}

TEST_CASE("appendEscapedString escapes other control bytes as \\u00XX") {
    std::string withNul = std::string("a") + '\x01' + "b";
    CHECK(escaped(withNul) == "\"a\\u0001b\"");
}

TEST_CASE("appendEscapedString handles untrusted SNMP-style binary junk without crashing") {
    // ifDescr et al. come straight off the wire from whatever the agent
    // sends; this is a rough stand-in for "not clean ASCII".
    std::string junk;
    for (int i = 0; i < 256; ++i) {
        junk.push_back(static_cast<char>(i));
    }
    std::string out = escaped(junk);
    CHECK(out.front() == '"');
    CHECK(out.back() == '"');
    // No unescaped control character or quote should survive into the
    // output body (between the surrounding quotes).
    std::string body = out.substr(1, out.size() - 2);
    for (size_t i = 0; i < body.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(body[i]);
        if (c < 0x20) {
            FAIL("raw control byte leaked into JSON output");
        }
        if (c == '"') {
            REQUIRE(i > 0);
            CHECK(body[i - 1] == '\\');
        }
    }
}
