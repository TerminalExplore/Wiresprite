#include "snmp/client.hpp"

#include <chrono>
#include <stdexcept>

#include "snmp/udp_socket.hpp"

namespace snmpmon {

namespace {

// Safety caps for walkSubtree, protecting against a buggy or malicious
// agent that never terminates a walk (e.g. never returns EndOfMibView
// and keeps repeating OIDs).
constexpr int kWalkMaxIterations = 1000;
constexpr int32_t kWalkMaxRepetitionsPerRequest = 10;

} // namespace

SnmpClient::SnmpClient(std::string host, uint16_t port, std::string community, SnmpVersion version)
    : host_(std::move(host)),
      port_(port),
      community_(std::move(community)),
      version_(version),
      requestIdRng_(std::random_device{}()) {}

int32_t SnmpClient::nextRequestId() {
    return requestIdDist_(requestIdRng_);
}

SnmpGetResult SnmpClient::sendAndReceive(SnmpMessage request) {
    request.version = version_;
    request.community = community_;
    request.requestId = nextRequestId();
    const int32_t requestId = request.requestId;
    std::string encoded = request.encode();

    UdpSocket socket;
    for (int attempt = 0; attempt <= retries_; ++attempt) {
        socket.sendTo(host_, port_, encoded);

        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs_);
        for (;;) {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                                                     std::chrono::steady_clock::now())
                                  .count();
            if (remaining <= 0) {
                break; // this attempt's window expired; outer loop resends
            }

            std::string data;
            std::string fromHost;
            uint16_t fromPort = 0;
            if (!socket.receiveFrom(data, fromHost, fromPort, static_cast<int>(remaining))) {
                break; // timed out
            }

            SnmpMessage response;
            try {
                response = SnmpMessage::decode(data);
            } catch (const ber::DecodeError&) {
                continue; // malformed/foreign packet on this port; keep waiting
            }
            if (response.pduType != PduType::GetResponse || response.requestId != requestId) {
                continue; // stray or late packet from an earlier attempt; keep waiting
            }

            SnmpGetResult result;
            result.errorStatus = response.errorStatus;
            result.errorIndex = response.errorIndex;
            result.varBinds = std::move(response.varBinds);
            return result;
        }
    }

    throw SnmpTimeoutError("SNMP request to " + host_ + ":" + std::to_string(port_) + " timed out after " +
                            std::to_string(retries_ + 1) + " attempt(s)");
}

SnmpGetResult SnmpClient::get(const std::vector<Oid>& oids) {
    SnmpMessage request;
    request.pduType = PduType::GetRequest;
    request.errorStatus = 0;
    request.errorIndex = 0;
    request.varBinds.reserve(oids.size());
    for (const auto& oid : oids) {
        request.varBinds.push_back(VarBind{oid, ber::Value::null()});
    }
    return sendAndReceive(std::move(request));
}

SnmpGetResult SnmpClient::getNext(const std::vector<Oid>& oids) {
    SnmpMessage request;
    request.pduType = PduType::GetNextRequest;
    request.errorStatus = 0;
    request.errorIndex = 0;
    request.varBinds.reserve(oids.size());
    for (const auto& oid : oids) {
        request.varBinds.push_back(VarBind{oid, ber::Value::null()});
    }
    return sendAndReceive(std::move(request));
}

SnmpGetResult SnmpClient::getBulk(int32_t nonRepeaters, int32_t maxRepetitions, const std::vector<Oid>& oids) {
    if (version_ != SnmpVersion::V2c) {
        throw std::logic_error("SnmpClient::getBulk requires SNMPv2c");
    }
    SnmpMessage request;
    request.pduType = PduType::GetBulkRequest;
    request.errorStatus = nonRepeaters;   // non-repeaters, per GETBULK's field reuse
    request.errorIndex = maxRepetitions;  // max-repetitions
    request.varBinds.reserve(oids.size());
    for (const auto& oid : oids) {
        request.varBinds.push_back(VarBind{oid, ber::Value::null()});
    }
    return sendAndReceive(std::move(request));
}

std::vector<VarBind> SnmpClient::walkSubtree(const Oid& base) {
    std::vector<VarBind> results;
    Oid current = base;

    for (int iteration = 0; iteration < kWalkMaxIterations; ++iteration) {
        SnmpGetResult response = (version_ == SnmpVersion::V2c)
                                      ? getBulk(0, kWalkMaxRepetitionsPerRequest, {current})
                                      : getNext({current});

        if (response.errorStatus != 0 || response.varBinds.empty()) {
            // SNMPv1 signals "walked past the end" with noSuchName
            // (errorStatus != 0) rather than a v2c exception value.
            break;
        }

        bool doneWithSubtree = false;
        for (auto& vb : response.varBinds) {
            if (vb.value.tag == ber::Tag::EndOfMibView) {
                doneWithSubtree = true;
                break;
            }
            if (!vb.name.isSubtreeOf(base)) {
                doneWithSubtree = true;
                break;
            }
            if (!results.empty() && !(results.back().name < vb.name)) {
                // Non-increasing OID: a misbehaving agent. Stop rather
                // than risk looping forever.
                doneWithSubtree = true;
                break;
            }
            current = vb.name;
            results.push_back(std::move(vb));
        }

        if (doneWithSubtree) {
            break;
        }
    }

    return results;
}

} // namespace snmpmon
