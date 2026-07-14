// Integration test: exercises the real OS network stack end to end
// (encode -> UDP send -> UDP receive -> decode) over loopback, using a
// small hand-rolled "fake agent" instead of depending on an external
// SNMP daemon or a real switch being reachable. This is the strongest
// verification Phase 2 can give without a live device: every layer
// (BER codec, PDU framing, UdpSocket, SnmpClient) runs for real, just
// against 127.0.0.1 instead of a physical switch.

#include "doctest.h"
#include "snmp/client.hpp"
#include "snmp/pdu.hpp"
#include "snmp/udp_socket.hpp"

#include <thread>

using namespace wiresprite;

TEST_CASE("SnmpClient::get round-trips through a real UDP loopback fake agent") {
    // The exact OID app/snmp_monitor.py's Python prototype queried
    // (ifInOctets.1), so this doubles as behavioral parity coverage.
    Oid targetOid = Oid::parse("1.3.6.1.2.1.2.2.1.10.1");
    constexpr uint32_t kCannedCounterValue = 123456;

    UdpSocket listener;
    listener.bind(0);
    uint16_t listenerPort = listener.localPort();

    std::thread agent([&]() {
        std::string data, fromHost;
        uint16_t fromPort = 0;
        if (!listener.receiveFrom(data, fromHost, fromPort, 5000)) {
            return; // test will fail on the client side via SnmpTimeoutError
        }

        SnmpMessage request = SnmpMessage::decode(data);

        SnmpMessage response;
        response.version = request.version;
        response.community = request.community;
        response.pduType = PduType::GetResponse;
        response.requestId = request.requestId;
        response.errorStatus = 0;
        response.errorIndex = 0;
        for (const auto& vb : request.varBinds) {
            response.varBinds.push_back(VarBind{vb.name, ber::Value::counter32(kCannedCounterValue)});
        }

        listener.sendTo(fromHost, fromPort, response.encode());
    });

    SnmpClient client("127.0.0.1", listenerPort, "public", SnmpVersion::V2c);
    client.setTimeoutMs(2000);
    client.setRetries(1);

    SnmpGetResult result = client.get({targetOid});
    agent.join();

    CHECK(result.errorStatus == 0);
    REQUIRE(result.varBinds.size() == 1);
    CHECK(result.varBinds[0].name == targetOid);
    CHECK(result.varBinds[0].value.tag == ber::Tag::Counter32);
    CHECK(result.varBinds[0].value.asUint() == kCannedCounterValue);
}

TEST_CASE("SnmpClient::get throws SnmpTimeoutError when nothing answers") {
    // No agent is started on this port, so every attempt must time out.
    // Bind-then-close to grab a port unlikely to have anything listening,
    // without needing an external "definitely closed" port guess.
    uint16_t freePort;
    {
        UdpSocket probe;
        probe.bind(0);
        freePort = probe.localPort();
    }

    SnmpClient client("127.0.0.1", freePort, "public", SnmpVersion::V2c);
    client.setTimeoutMs(150);
    client.setRetries(0);

    CHECK_THROWS_AS(client.get({Oid::parse("1.3.6.1.2.1.1.3.0")}), SnmpTimeoutError);
}
