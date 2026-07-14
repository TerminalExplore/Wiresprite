#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "snmp/ber.hpp"
#include "snmp/oid.hpp"

// SNMP message/PDU framing, built on top of the generic ber:: codec.
// This is the layer that knows about SNMP's context-specific constructed
// tags (GetRequest, GetResponse, ...) that ber.hpp deliberately doesn't.
namespace snmpmon {

enum class SnmpVersion : int32_t {
    V1 = 0,
    V2c = 1,
};

enum class PduType : uint8_t {
    GetRequest     = 0xA0,
    GetNextRequest = 0xA1,
    GetResponse    = 0xA2,
    SetRequest     = 0xA3,
    GetBulkRequest = 0xA5, // v2c only
};

struct VarBind {
    Oid name;
    ber::Value value; // ber::Value::null() when used in a request
};

struct SnmpMessage {
    SnmpVersion version = SnmpVersion::V2c;
    std::string community;
    PduType pduType = PduType::GetRequest;
    int32_t requestId = 0;
    // For GetRequest/GetNextRequest/GetResponse: SNMPv1/v2c error-status
    // and error-index. For GetBulkRequest (v2c only), these two fields
    // are reinterpreted on the wire as non-repeaters and max-repetitions
    // respectively — same SEQUENCE shape, different PDU-level meaning.
    int32_t errorStatus = 0;
    int32_t errorIndex = 0;
    std::vector<VarBind> varBinds;

    std::string encode() const;

    // Throws ber::DecodeError on malformed input.
    static SnmpMessage decode(const std::string& buf);
};

} // namespace snmpmon
