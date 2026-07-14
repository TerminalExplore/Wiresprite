#pragma once

// A minimal in-process SNMP agent simulator for integration tests: serves
// GetRequest/GetNextRequest/GetBulkRequest against a fixed table of
// (Oid, Value) pairs over a real UDP loopback socket, so SnmpClient and
// walkSubtree can be exercised end to end without a real device or an
// external snmpd. Test-only; not part of the shipped library.

#include <algorithm>
#include <atomic>
#include <utility>
#include <vector>

#include "snmp/pdu.hpp"
#include "snmp/udp_socket.hpp"

namespace wiresprite::test {

class FakeAgent {
public:
    // `sortedTable` must already be in ascending Oid order.
    explicit FakeAgent(std::vector<std::pair<Oid, ber::Value>> sortedTable) : table_(std::move(sortedTable)) {
        listener_.bind(0);
    }

    uint16_t port() const { return listener_.localPort(); }

    // How many requests this agent has decoded so far, for tests that
    // verify a poller actually re-polls on a cadence rather than once.
    size_t requestsServed() const { return requestsServed_.load(); }

    // Serves requests until `timeoutMs` passes with nothing received,
    // which is how it notices the client's walk has finished. Intended
    // to be run on its own std::thread.
    void serveUntilIdle(int timeoutMs = 2000) {
        for (;;) {
            std::string data;
            std::string fromHost;
            uint16_t fromPort = 0;
            if (!listener_.receiveFrom(data, fromHost, fromPort, timeoutMs)) {
                return;
            }
            SnmpMessage request = SnmpMessage::decode(data);
            requestsServed_.fetch_add(1);
            SnmpMessage response = buildResponse(request);
            listener_.sendTo(fromHost, fromPort, response.encode());
        }
    }

private:
    SnmpMessage buildResponse(const SnmpMessage& request) const {
        SnmpMessage response;
        response.version = request.version;
        response.community = request.community;
        response.pduType = PduType::GetResponse;
        response.requestId = request.requestId;
        response.errorStatus = 0;
        response.errorIndex = 0;

        if (request.pduType == PduType::GetRequest) {
            for (const auto& vb : request.varBinds) {
                auto it = std::find_if(table_.begin(), table_.end(),
                                        [&](const auto& row) { return row.first == vb.name; });
                response.varBinds.push_back(
                    VarBind{vb.name, it != table_.end() ? it->second : ber::Value::noSuchObject()});
            }
            return response;
        }

        // GetNextRequest and GetBulkRequest both mean "give me the next
        // entries after this OID"; GetBulk just asks for several at once.
        int32_t maxRepetitions = 1;
        if (request.pduType == PduType::GetBulkRequest) {
            maxRepetitions = request.errorIndex > 0 ? request.errorIndex : 1; // max-repetitions field
        }

        for (const auto& vb : request.varBinds) {
            auto it = std::upper_bound(table_.begin(), table_.end(), vb.name,
                                        [](const Oid& oid, const auto& row) { return oid < row.first; });
            int32_t produced = 0;
            for (; it != table_.end() && produced < maxRepetitions; ++it, ++produced) {
                response.varBinds.push_back(VarBind{it->first, it->second});
            }
            while (produced < maxRepetitions) {
                response.varBinds.push_back(VarBind{vb.name, ber::Value::endOfMibView()});
                ++produced;
            }
        }
        return response;
    }

    UdpSocket listener_;
    std::vector<std::pair<Oid, ber::Value>> table_;
    std::atomic<size_t> requestsServed_{0};
};

} // namespace wiresprite::test
