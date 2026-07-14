#pragma once

#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "snmp/pdu.hpp"

namespace snmpmon {

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

private:
    int32_t nextRequestId();

    std::string host_;
    uint16_t port_;
    std::string community_;
    SnmpVersion version_;
    int timeoutMs_ = 1500;
    int retries_ = 2;
    std::mt19937 requestIdRng_;
    std::uniform_int_distribution<int32_t> requestIdDist_{1, 0x7FFFFFFF};
};

} // namespace snmpmon
