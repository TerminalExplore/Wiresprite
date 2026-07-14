#pragma once

#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "snmp/pdu.hpp"

namespace wiresprite {

struct SnmpGetResult {
    int32_t errorStatus = 0;
    int32_t errorIndex = 0;
    std::vector<VarBind> varBinds;
};

class SnmpTimeoutError : public std::runtime_error {
public:
    explicit SnmpTimeoutError(const std::string& message) : std::runtime_error(message) {}
};

class SnmpClient {
public:
    SnmpClient(std::string host, uint16_t port, std::string community, SnmpVersion version = SnmpVersion::V2c);

    void setTimeoutMs(int timeoutMs) { timeoutMs_ = timeoutMs; }
    void setRetries(int retries) { retries_ = retries; }

    // Sends a single GetRequest covering all of `oids`. Ignores stray or
    // mismatched-requestId packets and keeps waiting within the current
    // attempt's timeout window; resends up to `retries` times. Throws
    // SnmpTimeoutError if no matching GetResponse ever arrives.
    SnmpGetResult get(const std::vector<Oid>& oids);

    // Same delivery semantics as get(), but sends a GetNextRequest.
    SnmpGetResult getNext(const std::vector<Oid>& oids);

    // Same delivery semantics as get(), but sends a GetBulkRequest.
    // SNMPv2c only; throws std::logic_error if this client is SNMPv1.
    SnmpGetResult getBulk(int32_t nonRepeaters, int32_t maxRepetitions, const std::vector<Oid>& oids);

    // Walks every leaf in `base`'s subtree, in ascending OID order.
    // Uses GETBULK for v2c or a GETNEXT loop for v1, stopping at the
    // subtree boundary, an EndOfMibView/error response, a non-increasing
    // OID from a misbehaving agent, or a fixed iteration safety cap —
    // whichever comes first. Propagates SnmpTimeoutError like the other
    // methods if a request mid-walk never gets a response; callers that
    // want a "device unreachable" result instead of an exception (e.g.
    // the poller) catch it there, same as they would around get().
    std::vector<VarBind> walkSubtree(const Oid& base);

private:
    int32_t nextRequestId();
    SnmpGetResult sendAndReceive(SnmpMessage request);

    std::string host_;
    uint16_t port_;
    std::string community_;
    SnmpVersion version_;
    int timeoutMs_ = 1500;
    int retries_ = 2;
    std::mt19937 requestIdRng_;
    std::uniform_int_distribution<int32_t> requestIdDist_{1, 0x7FFFFFFF};
};

} // namespace wiresprite
