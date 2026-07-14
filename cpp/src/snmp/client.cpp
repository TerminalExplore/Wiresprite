#include "snmp/client.hpp"

#include <chrono>

#include "snmp/udp_socket.hpp"

namespace snmpmon {

SnmpClient::SnmpClient(std::string host, uint16_t port, std::string community, SnmpVersion version)
    : host_(std::move(host)),
      port_(port),
      community_(std::move(community)),
      version_(version),
      requestIdRng_(std::random_device{}()) {}

int32_t SnmpClient::nextRequestId() {
    return requestIdDist_(requestIdRng_);
}

SnmpGetResult SnmpClient::get(const std::vector<Oid>& oids) {
    int32_t requestId = nextRequestId();

    SnmpMessage request;
    request.version = version_;
    request.community = community_;
    request.pduType = PduType::GetRequest;
    request.requestId = requestId;
    request.varBinds.reserve(oids.size());
    for (const auto& oid : oids) {
        request.varBinds.push_back(VarBind{oid, ber::Value::null()});
    }
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

    throw SnmpTimeoutError("SNMP GET to " + host_ + ":" + std::to_string(port_) + " timed out after " +
                            std::to_string(retries_ + 1) + " attempt(s)");
}

} // namespace snmpmon
