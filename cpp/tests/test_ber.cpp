#include "doctest.h"
#include "snmp/ber.hpp"
#include "snmp/oid.hpp"

using namespace wiresprite;
using namespace wiresprite::ber;

namespace {

std::string fromHex(const std::string& hex) {
    std::string bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        bytes.push_back(static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16)));
    }
    return bytes;
}

std::string toHex(const std::string& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string hex;
    hex.reserve(bytes.size() * 2);
    for (unsigned char b : bytes) {
        hex.push_back(digits[b >> 4]);
        hex.push_back(digits[b & 0xF]);
    }
    return hex;
}

} // namespace

// Every expected hex string below was cross-checked against pyasn1's BER
// encoder (pip install pyasn1), an independent implementation, rather
// than hand-derived — e.g.:
//   pyasn1.codec.ber.encoder.encode(univ.ObjectIdentifier('1.3.6.1.2.1.2.2.1.10.1'))
// For Counter32/Gauge32/TimeTicks/Counter64, only the tag byte differs
// from a plain INTEGER of the same value, so the content bytes are taken
// from pyasn1's Integer encoding with the SNMP application tag swapped in.
//
// Exception: pyasn1 encodes negative powers of two (-128, INT32_MIN) one
// byte longer than the X.690 8.3.2 minimal-length rule requires (a known
// quirk, reproduced by both its BER and "DER" encoders). Cross-checked
// against a second, independent library (asn1crypto) which agrees with
// this codec's minimal-length output instead: -128 => "020180" (1 byte),
// INT32_MIN => "020480000000" (4 bytes).

TEST_CASE("encodeLength / decodeLength short and long form") {
    CHECK(toHex(encodeLength(0)) == "00");
    CHECK(toHex(encodeLength(127)) == "7f");
    CHECK(toHex(encodeLength(128)) == "8180");
    CHECK(toHex(encodeLength(130)) == "8182");
    CHECK(toHex(encodeLength(200)) == "81c8");
    CHECK(toHex(encodeLength(65535)) == "82ffff");

    for (size_t length : {0u, 127u, 128u, 130u, 200u, 65535u}) {
        std::string encoded = encodeLength(length);
        size_t pos = 0;
        CHECK(decodeLength(encoded, pos) == length);
        CHECK(pos == encoded.size());
    }
}

TEST_CASE("decodeLength rejects indefinite form and truncated input") {
    std::string indefinite = fromHex("80");
    size_t pos = 0;
    CHECK_THROWS_AS(decodeLength(indefinite, pos), DecodeError);

    std::string truncated = fromHex("82ff"); // says 2 length bytes follow, only 1 given
    pos = 0;
    CHECK_THROWS_AS(decodeLength(truncated, pos), DecodeError);
}

TEST_CASE("Integer round-trip against pyasn1 reference vectors") {
    struct Case { int64_t value; const char* hex; };
    const Case cases[] = {
        {0, "020100"},
        {-1, "0201ff"},
        {127, "02017f"},
        {128, "02020080"},
        {-128, "020180"},
        {65535, "020300ffff"},
        {2147483647LL, "02047fffffff"},
        {-2147483648LL, "020480000000"},
    };
    for (const auto& c : cases) {
        CAPTURE(c.value);
        std::string encoded = encode(Value::integer(c.value));
        CHECK(toHex(encoded) == c.hex);

        size_t pos = 0;
        Value decoded = decode(encoded, pos);
        CHECK(pos == encoded.size());
        CHECK(decoded.tag == Tag::Integer);
        CHECK(decoded.asInt() == c.value);
    }
}

TEST_CASE("OctetString round-trip, including empty and binary content") {
    CHECK(toHex(encode(Value::octetString(""))) == "0400");
    CHECK(toHex(encode(Value::octetString("public"))) == "04067075626c6963");

    std::string binary = fromHex("00ff7f80");
    CHECK(toHex(encode(Value::octetString(binary))) == "040400ff7f80");

    std::string encoded = encode(Value::octetString(binary));
    size_t pos = 0;
    Value decoded = decode(encoded, pos);
    CHECK(decoded.tag == Tag::OctetString);
    CHECK(decoded.asOctetString() == binary);
}

TEST_CASE("Null round-trip") {
    std::string encoded = encode(Value::null());
    CHECK(toHex(encoded) == "0500");
    size_t pos = 0;
    Value decoded = decode(encoded, pos);
    CHECK(decoded.tag == Tag::Null);
}

TEST_CASE("ObjectIdentifier round-trip against pyasn1 reference vectors") {
    struct Case { const char* dotted; const char* hex; };
    const Case cases[] = {
        {"0.0", "060100"},
        {"1.3.6.1.2.1.2.2.1.10.1", "060a2b060102010202010a01"},
        {"1.3.6.1.2.1.1.3.0", "06082b06010201010300"},
        {"1.3.6.1.4.1.9.9.48", "06082b06010401090930"},
    };
    for (const auto& c : cases) {
        CAPTURE(c.dotted);
        Oid oid = Oid::parse(c.dotted);
        std::string encoded = encode(Value::objectIdentifier(oid));
        CHECK(toHex(encoded) == c.hex);

        size_t pos = 0;
        Value decoded = decode(encoded, pos);
        CHECK(pos == encoded.size());
        CHECK(decoded.asOid() == oid);
    }
}

TEST_CASE("Counter32 / Gauge32 / TimeTicks / Counter64 round-trip") {
    // Content bytes match Integer's for the same magnitude; only the tag differs.
    CHECK(toHex(encode(Value::counter32(0))) == "410100");
    CHECK(toHex(encode(Value::counter32(0xFFFFFFFF))) == "410500ffffffff");
    CHECK(toHex(encode(Value::gauge32(128))) == "42020080");
    CHECK(toHex(encode(Value::timeTicks(65535))) == "430300ffff");
    CHECK(toHex(encode(Value::counter64(0xFFFFFFFFFFFFFFFFULL))) == "460900ffffffffffffffff");

    auto roundTrip = [](const Value& v) {
        std::string encoded = encode(v);
        size_t pos = 0;
        return decode(encoded, pos);
    };

    Value c32 = roundTrip(Value::counter32(0xFFFFFFFF));
    CHECK(c32.tag == Tag::Counter32);
    CHECK(c32.asUint() == 0xFFFFFFFFULL);

    Value g32 = roundTrip(Value::gauge32(42));
    CHECK(g32.tag == Tag::Gauge32);
    CHECK(g32.asUint() == 42);

    Value tt = roundTrip(Value::timeTicks(123456));
    CHECK(tt.tag == Tag::TimeTicks);
    CHECK(tt.asUint() == 123456);

    Value c64 = roundTrip(Value::counter64(0xFFFFFFFFFFFFFFFFULL));
    CHECK(c64.tag == Tag::Counter64);
    CHECK(c64.asUint() == 0xFFFFFFFFFFFFFFFFULL);
}

TEST_CASE("Sequence nests and round-trips arbitrary values") {
    Value seq = Value::sequence({
        Value::objectIdentifier(Oid::parse("1.3.6.1.2.1.2.2.1.10.1")),
        Value::counter32(1234),
        Value::octetString("eth0"),
    });

    std::string encoded = encode(seq);
    size_t pos = 0;
    Value decoded = decode(encoded, pos);
    CHECK(pos == encoded.size());
    CHECK(decoded.tag == Tag::Sequence);

    const auto& children = decoded.asSequence();
    REQUIRE(children.size() == 3);
    CHECK(children[0].asOid() == Oid::parse("1.3.6.1.2.1.2.2.1.10.1"));
    CHECK(children[1].asUint() == 1234);
    CHECK(children[2].asOctetString() == "eth0");
}

TEST_CASE("SNMPv2c exception values round-trip") {
    for (Value v : {Value::noSuchObject(), Value::noSuchInstance(), Value::endOfMibView()}) {
        std::string encoded = encode(v);
        size_t pos = 0;
        Value decoded = decode(encoded, pos);
        CHECK(decoded.tag == v.tag);
        CHECK(pos == encoded.size());
    }
}

TEST_CASE("Wrong-accessor access throws DecodeError") {
    Value intVal = Value::integer(1);
    CHECK_THROWS_AS(intVal.asOctetString(), DecodeError);
    CHECK_THROWS_AS(intVal.asOid(), DecodeError);
    CHECK_THROWS_AS(intVal.asUint(), DecodeError);
    CHECK_THROWS_AS(intVal.asSequence(), DecodeError);
}

TEST_CASE("Full GET-request varbind mirrors the Python prototype's request") {
    // app/snmp_monitor.py's exact test case: OID 1.3.6.1.2.1.2.2.1.10.1
    // (ifInOctets.1), community "public". This is the OID Phase 2's live
    // SNMP GET test will target.
    Oid target = Oid::parse("1.3.6.1.2.1.2.2.1.10.1");
    Value varbind = Value::sequence({Value::objectIdentifier(target), Value::null()});
    std::string encoded = encode(varbind);

    // SEQUENCE content = full OID TLV (12 bytes: 06 0a <10 bytes>) + full
    // NULL TLV (2 bytes: 05 00) = 14 = 0x0e bytes.
    CHECK(toHex(encoded) == "300e060a2b060102010202010a010500");

    size_t pos = 0;
    Value decoded = decode(encoded, pos);
    const auto& children = decoded.asSequence();
    REQUIRE(children.size() == 2);
    CHECK(children[0].asOid() == target);
    CHECK(children[1].tag == Tag::Null);
}
