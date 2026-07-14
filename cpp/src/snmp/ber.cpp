#include "snmp/ber.hpp"

#include <cstddef>

namespace wiresprite::ber {

namespace {

// Minimal two's-complement big-endian encoding, the rule ASN.1 INTEGER
// (and by convention the APPLICATION-class SNMP counters/gauges) uses.
std::string encodeSignedMagnitude(int64_t value) {
    // Collect bytes little-endian first, then reverse; simplest way to
    // strip redundant leading 0x00 / 0xFF bytes uniformly for both signs.
    std::string bytes;
    uint64_t bits = static_cast<uint64_t>(value);
    do {
        bytes.push_back(static_cast<char>(bits & 0xFF));
        bits = static_cast<uint64_t>(static_cast<int64_t>(bits) >> 8);
    } while (!bytes.empty() &&
             !((bits == 0 && !(static_cast<uint8_t>(bytes.back()) & 0x80)) ||
               (bits == static_cast<uint64_t>(-1) && (static_cast<uint8_t>(bytes.back()) & 0x80))));
    std::string result(bytes.rbegin(), bytes.rend());
    return result;
}

int64_t decodeSignedMagnitude(const std::string& content) {
    if (content.empty()) {
        throw DecodeError("BER INTEGER: zero-length content");
    }
    int64_t value = (static_cast<uint8_t>(content[0]) & 0x80) ? -1 : 0;
    for (unsigned char c : content) {
        value = (value << 8) | c;
    }
    return value;
}

// Same minimal-length rule as encodeSignedMagnitude, but for values that
// are conceptually unsigned (Counter32/Gauge32/TimeTicks/Counter64): a
// leading 0x00 is still inserted when the top bit is set, purely to keep
// the value from being misread as negative by a generic INTEGER decoder.
std::string encodeUnsignedMagnitude(uint64_t value) {
    std::string bytes;
    do {
        bytes.push_back(static_cast<char>(value & 0xFF));
        value >>= 8;
    } while (value != 0);
    if (static_cast<uint8_t>(bytes.back()) & 0x80) {
        bytes.push_back(static_cast<char>(0x00));
    }
    return std::string(bytes.rbegin(), bytes.rend());
}

uint64_t decodeUnsignedMagnitude(const std::string& content) {
    if (content.empty()) {
        throw DecodeError("BER unsigned value: zero-length content");
    }
    uint64_t value = 0;
    for (unsigned char c : content) {
        value = (value << 8) | c;
    }
    return value;
}

std::string encodeOidBytes(const Oid& oid) {
    const auto& components = oid.components();
    if (components.size() < 2) {
        throw std::invalid_argument("BER OID encode: needs at least 2 components, got " +
                                     std::to_string(components.size()));
    }
    std::string bytes;
    bytes.push_back(static_cast<char>(components[0] * 40 + components[1]));
    for (size_t i = 2; i < components.size(); ++i) {
        uint32_t v = components[i];
        char chunk[5];
        int n = 0;
        do {
            chunk[n++] = static_cast<char>(v & 0x7F);
            v >>= 7;
        } while (v != 0);
        for (int j = n - 1; j >= 0; --j) {
            uint8_t b = static_cast<uint8_t>(chunk[j]);
            if (j != 0) {
                b |= 0x80;
            }
            bytes.push_back(static_cast<char>(b));
        }
    }
    return bytes;
}

Oid decodeOidBytes(const std::string& content) {
    if (content.empty()) {
        throw DecodeError("BER OID: zero-length content");
    }
    std::vector<uint32_t> components;
    uint8_t first = static_cast<uint8_t>(content[0]);
    components.push_back(first / 40);
    components.push_back(first % 40);

    uint32_t value = 0;
    bool inChunk = false;
    for (size_t i = 1; i < content.size(); ++i) {
        uint8_t b = static_cast<uint8_t>(content[i]);
        value = (value << 7) | (b & 0x7F);
        inChunk = true;
        if (!(b & 0x80)) {
            components.push_back(value);
            value = 0;
            inChunk = false;
        }
    }
    if (inChunk) {
        throw DecodeError("BER OID: truncated sub-identifier");
    }
    return Oid(std::move(components));
}

} // namespace

std::string encodeLength(size_t length) {
    if (length < 0x80) {
        return std::string(1, static_cast<char>(length));
    }
    std::string bytes;
    size_t remaining = length;
    while (remaining != 0) {
        bytes.push_back(static_cast<char>(remaining & 0xFF));
        remaining >>= 8;
    }
    std::string result(bytes.rbegin(), bytes.rend());
    return std::string(1, static_cast<char>(0x80 | result.size())) + result;
}

size_t decodeLength(const std::string& buf, size_t& pos) {
    if (pos >= buf.size()) {
        throw DecodeError("BER length: truncated");
    }
    uint8_t first = static_cast<uint8_t>(buf[pos++]);
    if (!(first & 0x80)) {
        return first;
    }
    size_t numBytes = first & 0x7F;
    if (numBytes == 0) {
        throw DecodeError("BER length: indefinite form is not supported");
    }
    if (pos + numBytes > buf.size()) {
        throw DecodeError("BER length: truncated long-form length");
    }
    size_t length = 0;
    for (size_t i = 0; i < numBytes; ++i) {
        length = (length << 8) | static_cast<uint8_t>(buf[pos++]);
    }
    return length;
}

std::string encodeTlv(uint8_t tag, const std::string& contents) {
    return std::string(1, static_cast<char>(tag)) + encodeLength(contents.size()) + contents;
}

RawTlv decodeTlv(const std::string& buf, size_t& pos) {
    if (pos >= buf.size()) {
        throw DecodeError("BER TLV: truncated tag");
    }
    RawTlv tlv;
    tlv.tag = static_cast<uint8_t>(buf[pos++]);
    size_t length = decodeLength(buf, pos);
    if (pos + length > buf.size()) {
        throw DecodeError("BER TLV: truncated contents");
    }
    tlv.contents = buf.substr(pos, length);
    pos += length;
    return tlv;
}

Value Value::integer(int64_t v) {
    Value val;
    val.tag = Tag::Integer;
    val.data = v;
    return val;
}

Value Value::octetString(std::string v) {
    Value val;
    val.tag = Tag::OctetString;
    val.data = std::move(v);
    return val;
}

Value Value::null() {
    Value val;
    val.tag = Tag::Null;
    val.data = std::monostate{};
    return val;
}

Value Value::objectIdentifier(Oid v) {
    Value val;
    val.tag = Tag::ObjectIdentifier;
    val.data = std::move(v);
    return val;
}

Value Value::counter32(uint32_t v) {
    Value val;
    val.tag = Tag::Counter32;
    val.data = static_cast<uint64_t>(v);
    return val;
}

Value Value::gauge32(uint32_t v) {
    Value val;
    val.tag = Tag::Gauge32;
    val.data = static_cast<uint64_t>(v);
    return val;
}

Value Value::timeTicks(uint32_t v) {
    Value val;
    val.tag = Tag::TimeTicks;
    val.data = static_cast<uint64_t>(v);
    return val;
}

Value Value::counter64(uint64_t v) {
    Value val;
    val.tag = Tag::Counter64;
    val.data = v;
    return val;
}

Value Value::sequence(std::vector<Value> v) {
    Value val;
    val.tag = Tag::Sequence;
    val.data = std::move(v);
    return val;
}

Value Value::noSuchObject() {
    Value val;
    val.tag = Tag::NoSuchObject;
    val.data = std::monostate{};
    return val;
}

Value Value::noSuchInstance() {
    Value val;
    val.tag = Tag::NoSuchInstance;
    val.data = std::monostate{};
    return val;
}

Value Value::endOfMibView() {
    Value val;
    val.tag = Tag::EndOfMibView;
    val.data = std::monostate{};
    return val;
}

int64_t Value::asInt() const {
    if (tag != Tag::Integer) {
        throw DecodeError("Value::asInt: wrong tag");
    }
    return std::get<int64_t>(data);
}

const std::string& Value::asOctetString() const {
    if (tag != Tag::OctetString) {
        throw DecodeError("Value::asOctetString: wrong tag");
    }
    return std::get<std::string>(data);
}

const Oid& Value::asOid() const {
    if (tag != Tag::ObjectIdentifier) {
        throw DecodeError("Value::asOid: wrong tag");
    }
    return std::get<Oid>(data);
}

uint64_t Value::asUint() const {
    if (tag != Tag::Counter32 && tag != Tag::Gauge32 && tag != Tag::TimeTicks && tag != Tag::Counter64) {
        throw DecodeError("Value::asUint: wrong tag");
    }
    return std::get<uint64_t>(data);
}

const std::vector<Value>& Value::asSequence() const {
    if (tag != Tag::Sequence) {
        throw DecodeError("Value::asSequence: wrong tag");
    }
    return std::get<std::vector<Value>>(data);
}

std::string encode(const Value& value) {
    switch (value.tag) {
        case Tag::Integer:
            return encodeTlv(static_cast<uint8_t>(Tag::Integer), encodeSignedMagnitude(std::get<int64_t>(value.data)));
        case Tag::OctetString:
            return encodeTlv(static_cast<uint8_t>(Tag::OctetString), std::get<std::string>(value.data));
        case Tag::Null:
            return encodeTlv(static_cast<uint8_t>(Tag::Null), "");
        case Tag::ObjectIdentifier:
            return encodeTlv(static_cast<uint8_t>(Tag::ObjectIdentifier), encodeOidBytes(std::get<Oid>(value.data)));
        case Tag::Counter32:
        case Tag::Gauge32:
        case Tag::TimeTicks:
        case Tag::Counter64:
            return encodeTlv(static_cast<uint8_t>(value.tag), encodeUnsignedMagnitude(std::get<uint64_t>(value.data)));
        case Tag::Sequence: {
            std::string contents;
            for (const auto& child : std::get<std::vector<Value>>(value.data)) {
                contents += encode(child);
            }
            return encodeTlv(static_cast<uint8_t>(Tag::Sequence), contents);
        }
        case Tag::NoSuchObject:
        case Tag::NoSuchInstance:
        case Tag::EndOfMibView:
            return encodeTlv(static_cast<uint8_t>(value.tag), "");
        case Tag::IpAddress:
        case Tag::Opaque:
            // Not produced by this codebase; content is opaque bytes if ever needed.
            return encodeTlv(static_cast<uint8_t>(value.tag), std::get<std::string>(value.data));
    }
    throw DecodeError("BER encode: unhandled tag");
}

Value decode(const std::string& buf, size_t& pos) {
    RawTlv tlv = decodeTlv(buf, pos);
    Tag tag = static_cast<Tag>(tlv.tag);

    switch (tag) {
        case Tag::Integer:
            return Value::integer(decodeSignedMagnitude(tlv.contents));
        case Tag::OctetString:
            return Value::octetString(tlv.contents);
        case Tag::Null:
            return Value::null();
        case Tag::ObjectIdentifier:
            return Value::objectIdentifier(decodeOidBytes(tlv.contents));
        case Tag::Counter32:
            return Value::counter32(static_cast<uint32_t>(decodeUnsignedMagnitude(tlv.contents)));
        case Tag::Gauge32:
            return Value::gauge32(static_cast<uint32_t>(decodeUnsignedMagnitude(tlv.contents)));
        case Tag::TimeTicks:
            return Value::timeTicks(static_cast<uint32_t>(decodeUnsignedMagnitude(tlv.contents)));
        case Tag::Counter64:
            return Value::counter64(decodeUnsignedMagnitude(tlv.contents));
        case Tag::Sequence: {
            std::vector<Value> children;
            size_t childPos = 0;
            while (childPos < tlv.contents.size()) {
                children.push_back(decode(tlv.contents, childPos));
            }
            return Value::sequence(std::move(children));
        }
        case Tag::NoSuchObject:
            return Value::noSuchObject();
        case Tag::NoSuchInstance:
            return Value::noSuchInstance();
        case Tag::EndOfMibView:
            return Value::endOfMibView();
        case Tag::IpAddress:
        case Tag::Opaque: {
            Value val;
            val.tag = tag;
            val.data = tlv.contents;
            return val;
        }
    }
    throw DecodeError("BER decode: unhandled tag 0x" + std::to_string(tlv.tag));
}

} // namespace wiresprite::ber
