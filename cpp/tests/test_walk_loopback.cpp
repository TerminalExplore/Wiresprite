// Integration test: SnmpClient::walkSubtree and pollIfTable driven over
// a real UDP loopback round trip against fake_snmp_agent.hpp's simulated
// ifTable, covering both the GETBULK (v2c) and GETNEXT-loop (v1) code
// paths — the strongest verification available without a reachable
// physical switch or an installable snmpd in this environment.

#include "doctest.h"
#include "fake_snmp_agent.hpp"
#include "poll/if_table.hpp"
#include "snmp/client.hpp"

#include <thread>

using namespace snmpmon;
using namespace snmpmon::test;

namespace {

Oid col(uint32_t column, uint32_t ifIndex) {
    return Oid::parse("1.3.6.1.2.1.2.2.1").withSuffix({column, ifIndex});
}

// A 2-row simulated ifTable, already in ascending (column-major) OID
// order as FakeAgent requires and a real agent would return.
std::vector<std::pair<Oid, ber::Value>> makeTwoRowIfTable() {
    return {
        {col(2, 1), ber::Value::octetString("eth0")},
        {col(2, 2), ber::Value::octetString("eth1")},
        {col(3, 1), ber::Value::integer(6)},
        {col(3, 2), ber::Value::integer(6)},
        {col(5, 1), ber::Value::gauge32(100000000)},
        {col(5, 2), ber::Value::gauge32(1000000000)},
        {col(7, 1), ber::Value::integer(1)},
        {col(7, 2), ber::Value::integer(1)},
        {col(8, 1), ber::Value::integer(1)},
        {col(8, 2), ber::Value::integer(2)},
        {col(10, 1), ber::Value::counter32(1000)},
        {col(10, 2), ber::Value::counter32(2000)},
        {col(16, 1), ber::Value::counter32(500)},
        {col(16, 2), ber::Value::counter32(2500)},
    };
}

void checkTwoRowResult(const std::vector<IfEntry>& entries) {
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].ifIndex == 1);
    CHECK(entries[0].ifDescr == "eth0");
    CHECK(entries[0].ifSpeed == 100000000);
    CHECK(entries[0].ifOperStatus == 1);
    CHECK(entries[0].ifInOctets == 1000);
    CHECK(entries[0].ifOutOctets == 500);

    CHECK(entries[1].ifIndex == 2);
    CHECK(entries[1].ifDescr == "eth1");
    CHECK(entries[1].ifSpeed == 1000000000);
    CHECK(entries[1].ifOperStatus == 2);
    CHECK(entries[1].ifInOctets == 2000);
    CHECK(entries[1].ifOutOctets == 2500);
}

} // namespace

TEST_CASE("SnmpClient::walkSubtree over GETBULK (v2c) matches the simulated ifTable") {
    FakeAgent agent(makeTwoRowIfTable());
    std::thread agentThread([&] { agent.serveUntilIdle(); });

    SnmpClient client("127.0.0.1", agent.port(), "public", SnmpVersion::V2c);
    client.setTimeoutMs(1000);
    client.setRetries(1);

    std::vector<VarBind> varbinds = client.walkSubtree(Oid::parse("1.3.6.1.2.1.2.2.1"));
    agentThread.join();

    CHECK(varbinds.size() == 14);
    checkTwoRowResult(bucketIfTableVarBinds(varbinds));
}

TEST_CASE("SnmpClient::walkSubtree over a GETNEXT loop (v1) matches the simulated ifTable") {
    FakeAgent agent(makeTwoRowIfTable());
    std::thread agentThread([&] { agent.serveUntilIdle(); });

    SnmpClient client("127.0.0.1", agent.port(), "public", SnmpVersion::V1);
    client.setTimeoutMs(1000);
    client.setRetries(1);

    std::vector<VarBind> varbinds = client.walkSubtree(Oid::parse("1.3.6.1.2.1.2.2.1"));
    agentThread.join();

    CHECK(varbinds.size() == 14);
    checkTwoRowResult(bucketIfTableVarBinds(varbinds));
}

TEST_CASE("pollIfTable reports reachable == false for an unreachable device instead of throwing") {
    uint16_t freePort;
    {
        UdpSocket probe;
        probe.bind(0);
        freePort = probe.localPort();
    }

    SnmpClient client("127.0.0.1", freePort, "public", SnmpVersion::V2c);
    client.setTimeoutMs(100);
    client.setRetries(0);

    DevicePollResult result = pollIfTable(client);
    CHECK_FALSE(result.reachable);
    CHECK_FALSE(result.error.empty());
    CHECK(result.interfaces.empty());
}
