#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "snmp/oid.hpp"

// Minimal ASN.1 BER encoder/decoder covering exactly the types SNMP v1/v2c
// needs. This layer knows nothing about SNMP PDUs or sockets — it only
// turns typed values into bytes and back, so it can be unit-tested in
// isolation against known-good vectors.
namespace wiresprite::ber {

// Tag octets as used on the wire. The APPLICATION-class types
// (Counter32/Gauge32/TimeTicks/Counter64) share INTEGER's minimal
// two's-complement content encoding; only the tag differs.
enum class Tag : uint8_t {
    Integer          = 0x02,
    OctetString      = 0x04,
    Null             = 0x05,
    ObjectIdentifier = 0x06,
    Sequence         = 0x30,
    IpAddress        = 0x40,
    Counter32        = 0x41,
    Gauge32          = 0x42,
    TimeTicks        = 0x43,
    Opaque           = 0x44,
    Counter64        = 0x46,
    // SNMPv2c exception values a GETNEXT/GETBULK response can carry
    // in place of a real value (e.g. walking past the end of a table).
    NoSuchObject     = 0x80,
    NoSuchInstance   = 0x81,
    EndOfMibView     = 0x82,
};

class DecodeError : public std::runtime_error {
public:
    explicit DecodeError(const std::string& message) : std::runtime_error(message) {}
};

// A decoded (or to-be-encoded) value, tagged so that types with
// identical content encoding but different wire tags (Counter32 vs
// Gauge32 vs TimeTicks) stay distinguishable.
struct Value {
    Tag tag = Tag::Null;
    std::variant<std::monostate, int64_t, std::string, Oid, uint64_t, std::vector<Value>> data;

    static Value integer(int64_t v);
    static Value octetString(std::string v);
    static Value null();
    static Value objectIdentifier(Oid v);
    static Value counter32(uint32_t v);
    static Value gauge32(uint32_t v);
    static Value timeTicks(uint32_t v);
    static Value counter64(uint64_t v);
    static Value sequence(std::vector<Value> v);
    static Value noSuchObject();
    static Value noSuchInstance();
    static Value endOfMibView();

    // Each throws DecodeError if `tag` doesn't match the accessor.
    int64_t asInt() const;
    const std::string& asOctetString() const;
    const Oid& asOid() const;
    uint64_t asUint() const; // Counter32 / Gauge32 / TimeTicks / Counter64
    const std::vector<Value>& asSequence() const;
};

// Low-level generic TLV, exposed for the future PDU layer, which needs
// to wrap varbind sequences in context-specific constructed tags
// (GetRequest 0xA0, GetResponse 0xA2, GetBulkRequest 0xA5, ...) that
// this module has no reason to know about.
struct RawTlv {
    uint8_t tag = 0;
    std::string contents;
};

std::string encodeLength(size_t length);

// Reads a length field starting at buf[pos], advances pos past it, and
// returns the decoded length. Throws DecodeError on truncated or
// unsupported (indefinite-form) input.
size_t decodeLength(const std::string& buf, size_t& pos);

std::string encodeTlv(uint8_t tag, const std::string& contents);

// Reads one TLV starting at buf[pos] and advances pos past it.
RawTlv decodeTlv(const std::string& buf, size_t& pos);

std::string encode(const Value& value);

// Reads one typed value starting at buf[pos] and advances pos past it.
Value decode(const std::string& buf, size_t& pos);

} // namespace wiresprite::ber
